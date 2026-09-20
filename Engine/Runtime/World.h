#pragma once

#include <cstdint>

#include "Core/Math/MathTypes.h"
#include "Resources/ResourceHandle.h"
#include "Runtime/Bounds.h"
#include "Runtime/ComponentType.h"
#include "Runtime/Components/CameraComponent.h"
#include "Runtime/Components/LightComponent.h"
#include "Runtime/Components/NameComponent.h"
#include "Runtime/Components/RenderableComponent.h"
#include "Runtime/Components/TransformComponent.h"
#include "Runtime/Entity.h"
#include "Runtime/SceneObject.h"

namespace noc
{
    class IAllocator;
    class LinearArena;
    class ReflectionRegistry;

    struct RenderQueue;

    struct WorldStats
    {
        uint32_t visible = 0;
        uint32_t total = 0;
    };

    // Runtime world owner/orchestrator.
    //
    // Phase 15 removes the old parallel-array object model from World. Entity
    // identity is owned by EntityRegistry; each component domain owns its dense
    // storage. Renderer consumption remains an extracted frame-data boundary.
    class World
    {
    public:
        World() = default;
        ~World();

        World(const World&) = delete;
        World& operator=(const World&) = delete;
        World(World&&) = delete;
        World& operator=(World&&) = delete;

        bool Init(
            IAllocator& persistentAlloc,
            const ReflectionRegistry& reflection);
        void Shutdown();

        // Updates transform propagation and derived renderable bounds.
        void Update();

        // --- Entity model ---
        [[nodiscard]] EntityHandle CreateEntity();
        [[nodiscard]] bool DestroyEntity(EntityHandle entity);
        [[nodiscard]] bool IsAlive(EntityHandle entity) const;
        [[nodiscard]] uint32_t AliveCount() const;
        [[nodiscard]] uint32_t EntityCapacity() const;
        [[nodiscard]] EntityHandle EntityAtIndex(uint32_t index) const;

        // Phase 14 compatibility: CreateObject creates an entity with Transform.
        [[nodiscard]] SceneObjectHandle CreateObject();
        void DestroyObject(SceneObjectHandle entity);

        // --- Transform component ---
        [[nodiscard]] bool AddTransform(EntityHandle entity);
        [[nodiscard]] bool RemoveTransform(EntityHandle entity);
        [[nodiscard]] bool HasTransform(EntityHandle entity) const;
        [[nodiscard]] const TransformComponent* GetTransform(EntityHandle entity) const;

        [[nodiscard]] bool SetLocalTRS(
            SceneObjectHandle entity,
            const Vec3& translation,
            const Quat& rotation,
            const Vec3& scale);

        [[nodiscard]] bool SetParent(
            SceneObjectHandle child,
            SceneObjectHandle parent);

        [[nodiscard]] EntityHandle ParentOf(
            EntityHandle entity) const;
        [[nodiscard]] EntityHandle FirstChildOf(
            EntityHandle entity) const;
        [[nodiscard]] EntityHandle NextSiblingOf(
            EntityHandle entity) const;

        [[nodiscard]] Mat4 GetWorldMatrix(SceneObjectHandle entity);

        // --- Renderable component ---
        [[nodiscard]] bool AddRenderable(
            EntityHandle entity,
            ResourceHandle mesh,
            const AABB& localBounds);

        [[nodiscard]] bool RemoveRenderable(EntityHandle entity);
        [[nodiscard]] bool HasRenderable(EntityHandle entity) const;
        [[nodiscard]] const RenderableComponent* GetRenderable(EntityHandle entity) const;

        [[nodiscard]] bool SetRenderable(
            SceneObjectHandle entity,
            ResourceHandle mesh,
            const AABB& localBounds);

        [[nodiscard]] bool SetRenderableMesh(
            EntityHandle entity,
            ResourceHandle mesh);
        [[nodiscard]] bool SetRenderableLocalBounds(
            EntityHandle entity,
            const AABB& localBounds);
        [[nodiscard]] bool SetRenderableEnabled(
            EntityHandle entity,
            bool enabled);

        // --- Camera component ---
        [[nodiscard]] bool AddCamera(EntityHandle entity);
        [[nodiscard]] bool RemoveCamera(EntityHandle entity);
        [[nodiscard]] bool HasCamera(EntityHandle entity) const;
        [[nodiscard]] const CameraComponent* GetCamera(EntityHandle entity) const;

        // Compatibility policy used by Phase 14: lens settings are retained as
        // defaults and applied to the active camera selected by SetCameraFromObject.
        [[nodiscard]] bool SetCameraParams(
            float fovYRadians,
            float aspect,
            float nearZ,
            float farZ);

        [[nodiscard]] bool SetCameraPerspective(
            EntityHandle entity,
            float fovYRadians,
            float aspect,
            float nearZ,
            float farZ);
        [[nodiscard]] bool SetCameraEnabled(
            EntityHandle entity,
            bool enabled);

        [[nodiscard]] bool SetCameraFromObject(SceneObjectHandle entity);
        [[nodiscard]] EntityHandle ActiveCamera() const;

        // --- Light component ---
        [[nodiscard]] bool AddLight(
            EntityHandle entity,
            LightType type = LightType::Point);
        [[nodiscard]] bool RemoveLight(EntityHandle entity);
        [[nodiscard]] bool HasLight(EntityHandle entity) const;
        [[nodiscard]] const LightComponent* GetLight(EntityHandle entity) const;

        [[nodiscard]] bool SetLightType(EntityHandle entity, LightType type);
        [[nodiscard]] bool SetLightColor(EntityHandle entity, const Vec3& color);
        [[nodiscard]] bool SetLightIntensity(EntityHandle entity, float intensity);
        [[nodiscard]] bool SetLightRange(EntityHandle entity, float range);
        [[nodiscard]] bool SetLightSpotAngles(
            EntityHandle entity,
            float innerConeRadians,
            float outerConeRadians);
        [[nodiscard]] bool SetLightEnabled(EntityHandle entity, bool enabled);

        // --- Name component ---
        [[nodiscard]] bool AddName(EntityHandle entity, const char* name = "");
        [[nodiscard]] bool RemoveName(EntityHandle entity);
        [[nodiscard]] bool HasName(EntityHandle entity) const;
        [[nodiscard]] bool SetName(EntityHandle entity, const char* name);
        [[nodiscard]] const NameComponent* GetName(EntityHandle entity) const;

        // --- Component metadata ---
        [[nodiscard]] const ComponentTypeMetadata* FindComponentType(
            ComponentTypeId typeId) const;

        // --- Visibility / render extraction ---
        void SetCullingEnabled(bool enabled) { cullingEnabled_ = enabled; }
        [[nodiscard]] bool IsCullingEnabled() const { return cullingEnabled_; }
        void SetCullingMaxDistance(float meters);

        RenderQueue BuildRenderQueue(
            LinearArena& frameArena,
            uint32_t viewportW,
            uint32_t viewportH);

        [[nodiscard]] const WorldStats& GetLastStats() const;
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
