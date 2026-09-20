#pragma once

#include <cstdint>

#include "Core/Math/MathTypes.h"
#include "Resources/ResourceHandle.h"
#include "Runtime/Bounds.h"
#include "Runtime/ComponentType.h"
#include "Runtime/Components/CameraComponent.h"
#include "Runtime/Components/NameComponent.h"
#include "Runtime/Components/RenderableComponent.h"
#include "Runtime/Components/TransformComponent.h"
#include "Runtime/Entity.h"
#include "Runtime/SceneObject.h"

namespace noc
{
    class IAllocator;
    class LinearArena;

    struct RenderQueue;

    /** @brief Last render-extraction visibility statistics produced by World. */
    struct WorldStats
    {
        /** Number of renderables kept after current culling rules. */
        uint32_t visible = 0;
        /** Number of enabled renderables that had the required Transform. */
        uint32_t total = 0;
    };

    /**
     * @brief Authoritative runtime owner/orchestrator for entity-component world state.
     *
     * World owns the EntityRegistry plus the foundation Transform, Renderable, Camera
     * and Name component systems. Editor and renderer consume this same runtime state;
     * there is no second authoritative editor scene model.
     *
     * @par When to use
     * Use World for runtime entity lifecycle, component operations, transform hierarchy,
     * active camera selection and render extraction.
     *
     * @par Entity creation choices
     * CreateEntity() creates identity only.
     * CreateObject() is a compatibility helper that creates identity plus Transform.
     * Prefer the more explicit CreateEntity()+Add* pattern in new ECS-oriented code when
     * the required component set should be obvious.
     *
     * @par Ownership
     * World owns entity/component storage. Pointers returned by GetTransform(),
     * GetRenderable(), GetCamera() and GetName() are borrowed views. Because component
     * storage is dense and structural mutation may relocate entries, do not retain those
     * pointers across add/remove/destroy operations.
     *
     * @par Threading
     * Structural entity/component mutation is a main/runtime-thread operation in the
     * current architecture. World does not advertise general concurrent mutation safety.
     *
     * @par Derived state
     * Local Transform TRS is authoritative; world matrices and renderable world bounds
     * are derived. Update() propagates transforms and derived bounds. BuildRenderQueue()
     * also refreshes derived state before extraction.
     *
     * @par Typical entity setup
     * @code
     * auto& world = engine.GetWorld();
     * noc::EntityHandle e = world.CreateEntity();
     *
     * if (!e.IsValid() ||
     *     !world.AddTransform(e) ||
     *     !world.AddName(e, "Crate"))
     * {
     *     if (world.IsAlive(e)) world.DestroyEntity(e);
     * }
     * @endcode
     *
     * @see EntityHandle
     * @see EntityRegistry
     * @see RenderQueue
     * @ingroup world_ecs
     */
    class World
    {
    public:
        /** @brief Constructs an uninitialized World. Call Init() before entity/component use. */
        World() = default;

        /** @brief Destroys the World after defensively calling Shutdown(). */
        ~World();

        World(const World&) = delete;
        World& operator=(const World&) = delete;
        World(World&&) = delete;
        World& operator=(World&&) = delete;

        /**
         * @brief Initializes entity/component systems using persistent storage.
         * @param persistentAlloc Allocator that must outlive World until Shutdown().
         * @return true on success; false if any required registry/system cannot initialize.
         */
        bool Init(IAllocator& persistentAlloc);

        /**
         * @brief Releases all entities, components and ECS registries.
         *
         * Repeated calls after shutdown are harmless no-ops.
         */
        void Shutdown();

        /**
         * @brief Updates transform propagation and derived renderable world bounds.
         *
         * Call once per normal runtime frame before systems that require current derived
         * transforms/bounds. Engine::Tick() already performs this in the standard loop.
         */
        void Update();

        // --- Entity model ---

        /**
         * @brief Creates a live entity identity with no components.
         * @return New handle, or EntityHandle::Invalid() if World is not initialized or
         * identity allocation fails.
         */
        [[nodiscard]] EntityHandle CreateEntity();

        /**
         * @brief Removes foundation components and destroys one live entity.
         *
         * Current teardown order removes Camera, Renderable, Name and Transform while
         * identity is still valid, then destroys the EntityRegistry slot.
         *
         * @return true when a live entity was destroyed; false for invalid/stale/dead input.
         *
         * @warning All component pointers previously obtained for this entity become invalid.
         */
        [[nodiscard]] bool DestroyEntity(EntityHandle entity);

        /** @brief Returns whether the handle currently names a live entity. */
        [[nodiscard]] bool IsAlive(EntityHandle entity) const;

        /** @brief Returns the number of live entities. */
        [[nodiscard]] uint32_t AliveCount() const;

        /**
         * @brief Returns EntityRegistry slot capacity.
         * @warning Capacity is not the live count; unused/dead holes may exist.
         */
        [[nodiscard]] uint32_t EntityCapacity() const;

        /**
         * @brief Deterministically inspects the entity slot at @p index.
         * @return Live handle or EntityHandle::Invalid() for dead/unused/out-of-range slots.
         */
        [[nodiscard]] EntityHandle EntityAtIndex(uint32_t index) const;

        /**
         * @brief Compatibility helper: creates an entity and adds Transform.
         *
         * @return New entity-with-Transform or EntityHandle::Invalid() if either step fails.
         *
         * @note New ECS code may prefer CreateEntity() followed by explicit component adds.
         */
        [[nodiscard]] SceneObjectHandle CreateObject();

        /** @brief Compatibility wrapper around DestroyEntity(); ignores the boolean result. */
        void DestroyObject(SceneObjectHandle entity);

        // --- Transform component ---

        /**
         * @brief Adds a default Transform component to a live entity.
         * @return true on success; false for invalid/dead entity or duplicate/add failure.
         */
        [[nodiscard]] bool AddTransform(EntityHandle entity);

        /**
         * @brief Removes Transform from a live entity.
         *
         * If this entity is the active camera, active-camera selection is cleared first.
         *
         * @return true when a Transform was removed; false otherwise.
         */
        [[nodiscard]] bool RemoveTransform(EntityHandle entity);

        /** @brief Returns whether a live entity currently has Transform. */
        [[nodiscard]] bool HasTransform(EntityHandle entity) const;

        /**
         * @brief Returns the entity Transform as a borrowed read-only view.
         * @return Pointer or nullptr when World/entity/component is invalid/absent.
         *
         * @warning Do not retain across structural component mutation.
         */
        [[nodiscard]] const TransformComponent* GetTransform(EntityHandle entity) const;

        /**
         * @brief Replaces the local translation, rotation and scale of an existing Transform.
         *
         * Derived world transforms are updated by transform propagation.
         *
         * @return true when the Transform system accepts the mutation; false otherwise.
         */
        [[nodiscard]] bool SetLocalTRS(
            SceneObjectHandle entity,
            const Vec3& translation,
            const Quat& rotation,
            const Vec3& scale);

        /**
         * @brief Reparents one entity in the Transform hierarchy.
         *
         * @param child Live child entity.
         * @param parent Desired parent. Any invalid/dead handle is interpreted as
         * "detach to root" for Phase-14 compatibility.
         * @return true when the hierarchy mutation is accepted.
         *
         * @note TransformSystem rejects illegal hierarchy relationships such as cycles.
         */
        [[nodiscard]] bool SetParent(
            SceneObjectHandle child,
            SceneObjectHandle parent);

        /**
         * @brief Returns the derived world matrix for an entity Transform.
         * @return World matrix, or identity when World/Transform lookup fails.
         */
        [[nodiscard]] Mat4 GetWorldMatrix(SceneObjectHandle entity);

        // --- Renderable component ---

        /**
         * @brief Adds Renderable with mesh handle and local-space bounds.
         *
         * A Renderable without Transform is legal storage but is not render-extractable
         * until the entity also has Transform.
         *
         * @return true on success; false for invalid/dead entity, duplicate component or
         * component initialization failure.
         */
        [[nodiscard]] bool AddRenderable(
            EntityHandle entity,
            ResourceHandle mesh,
            const AABB& localBounds);

        /** @brief Removes Renderable from a live entity. */
        [[nodiscard]] bool RemoveRenderable(EntityHandle entity);

        /** @brief Returns whether a live entity has Renderable. */
        [[nodiscard]] bool HasRenderable(EntityHandle entity) const;

        /**
         * @brief Returns Renderable as a borrowed read-only view.
         * @warning Do not retain across structural component mutation.
         */
        [[nodiscard]] const RenderableComponent* GetRenderable(EntityHandle entity) const;

        /**
         * @brief Adds or updates an entity's mesh and local bounds.
         *
         * If Renderable is absent, this calls AddRenderable(); otherwise it updates the
         * existing component.
         *
         * @return true when the resulting renderable state is accepted.
         */
        [[nodiscard]] bool SetRenderable(
            SceneObjectHandle entity,
            ResourceHandle mesh,
            const AABB& localBounds);

        /**
         * @brief Enables/disables an existing Renderable for extraction.
         * @return true when the component exists and its enabled state is updated.
         */
        [[nodiscard]] bool SetRenderableEnabled(EntityHandle entity, bool enabled);

        // --- Camera component ---

        /** @brief Adds a Camera component to a live entity without making it active. */
        [[nodiscard]] bool AddCamera(EntityHandle entity);

        /** @brief Removes Camera from a live entity. */
        [[nodiscard]] bool RemoveCamera(EntityHandle entity);

        /** @brief Returns whether a live entity has Camera. */
        [[nodiscard]] bool HasCamera(EntityHandle entity) const;

        /**
         * @brief Returns Camera as a borrowed read-only view.
         * @warning Do not retain across structural component mutation.
         */
        [[nodiscard]] const CameraComponent* GetCamera(EntityHandle entity) const;

        /**
         * @brief Sets default perspective parameters and applies them to the active camera.
         *
         * Valid perspective requires:
         * - 0 < fovYRadians < pi
         * - aspect > 0
         * - nearZ > 0
         * - farZ > nearZ
         *
         * When no active camera exists, valid values are retained as defaults for a later
         * SetCameraFromObject().
         *
         * @return false for invalid perspective values or if updating the active camera
         * fails; true otherwise.
         */
        [[nodiscard]] bool SetCameraParams(
            float fovYRadians,
            float aspect,
            float nearZ,
            float farZ);

        /**
         * @brief Makes an entity the active camera, adding Camera if required.
         *
         * The entity must be alive and have Transform. Current default perspective
         * parameters are applied before activation.
         *
         * @return true when the entity becomes active camera; false on validation/setup failure.
         */
        [[nodiscard]] bool SetCameraFromObject(SceneObjectHandle entity);

        /** @brief Returns the active camera entity or EntityHandle::Invalid(). */
        [[nodiscard]] EntityHandle ActiveCamera() const;

        // --- Name component ---

        /**
         * @brief Adds a display Name component to a live entity.
         * @param name UTF-8 display name; may be empty.
         * @return true on success.
         *
         * @note Name is presentation/authoring data, not entity identity.
         */
        [[nodiscard]] bool AddName(EntityHandle entity, const char* name = "");

        /** @brief Removes Name from a live entity. */
        [[nodiscard]] bool RemoveName(EntityHandle entity);

        /** @brief Returns whether a live entity has Name. */
        [[nodiscard]] bool HasName(EntityHandle entity) const;

        /** @brief Replaces the display name of an existing Name component. */
        [[nodiscard]] bool SetName(EntityHandle entity, const char* name);

        /**
         * @brief Returns Name as a borrowed read-only view.
         * @warning Do not retain across structural component mutation.
         */
        [[nodiscard]] const NameComponent* GetName(EntityHandle entity) const;

        // --- Component metadata ---

        /**
         * @brief Looks up registered component-type metadata by stable ComponentTypeId.
         * @return Borrowed metadata pointer, or nullptr when unknown/uninitialized.
         */
        [[nodiscard]] const ComponentTypeMetadata* FindComponentType(
            ComponentTypeId typeId) const;

        // --- Visibility / render extraction ---

        /** @brief Enables/disables frustum/distance culling during render extraction. */
        void SetCullingEnabled(bool enabled) { cullingEnabled_ = enabled; }

        /** @brief Returns whether render-extraction culling is enabled. */
        [[nodiscard]] bool IsCullingEnabled() const { return cullingEnabled_; }

        /**
         * @brief Sets optional maximum culling distance in world units/meters.
         *
         * Negative values are clamped to 0. A value <= 0 disables the distance-limit
         * portion while frustum culling may still remain enabled.
         */
        void SetCullingMaxDistance(float meters);

        /**
         * @brief Extracts renderer-facing frame data from current ECS state.
         *
         * BuildRenderQueue refreshes derived transforms/bounds, derives the active view,
         * performs culling, then allocates visible RenderInstance entries from
         * @p frameArena in deterministic entity-index order.
         *
         * @param frameArena Per-frame arena that owns the returned instance array.
         * @param viewportW Current viewport width.
         * @param viewportH Current viewport height.
         * @return By-value RenderQueue whose @c instances pointer refers to frameArena.
         *
         * @warning Queue instance memory becomes invalid when that arena is reset
         * (normally the next Engine::BeginFrame()).
         *
         * @note On frame-arena allocation failure, the returned queue contains no
         * instances and an error is logged.
         */
        RenderQueue BuildRenderQueue(
            LinearArena& frameArena,
            uint32_t viewportW,
            uint32_t viewportH);

        /** @brief Returns statistics from the most recent BuildRenderQueue(). */
        [[nodiscard]] const WorldStats& GetLastStats() const;

        /**
         * @brief Requests one diagnostic culling dump on the next extraction.
         *
         * The next BuildRenderQueue() logs per-entity/summary culling information and
         * clears the request.
         */
        void DebugRequestCullDump();

    private:
        struct Impl;
        Impl* impl_ = nullptr;

        float cullingMaxDistance_ = 0.0f;
        WorldStats lastStats_{};

        bool cullingEnabled_ = true;
        bool debugCullDump_ = false;
    };
}
