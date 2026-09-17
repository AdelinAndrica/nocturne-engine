#include "Runtime/CameraSystem.h"

#include "Runtime/EntityRegistry.h"
#include "Runtime/TransformSystem.h"

namespace noc
{
    namespace
    {
        constexpr float kMinimumBasisLengthSq = 1.0e-10f;
        constexpr float kPi = 3.14159265358979323846f;
    }

    CameraSystem::~CameraSystem()
    {
        Shutdown();
    }

    bool CameraSystem::Init(
        EntityRegistry& entities,
        TransformSystem& transforms,
        IAllocator& allocator,
        uint32_t initialCapacity)
    {
        if (entities_)
            return true;

        const uint32_t sparseCapacity =
            entities.Capacity() > initialCapacity
                ? entities.Capacity()
                : initialCapacity;

        if (!cameras_.Init(
                allocator,
                initialCapacity,
                sparseCapacity))
        {
            return false;
        }

        entities_ = &entities;
        transforms_ = &transforms;
        activeCamera_ = EntityHandle::Invalid();
        return true;
    }

    void CameraSystem::Shutdown()
    {
        activeCamera_ = EntityHandle::Invalid();
        cameras_.Shutdown();
        transforms_ = nullptr;
        entities_ = nullptr;
    }

    CameraComponent* CameraSystem::Add(EntityHandle entity)
    {
        if (!IsUsableEntity_(entity))
            return nullptr;

        return cameras_.Emplace(entity);
    }

    bool CameraSystem::Remove(EntityHandle entity)
    {
        if (!IsUsableEntity_(entity))
            return false;

        if (activeCamera_ == entity)
            activeCamera_ = EntityHandle::Invalid();

        return cameras_.Remove(entity);
    }

    bool CameraSystem::Has(EntityHandle entity) const
    {
        return IsUsableEntity_(entity) && cameras_.Has(entity);
    }

    const CameraComponent* CameraSystem::Get(EntityHandle entity) const
    {
        if (!IsUsableEntity_(entity))
            return nullptr;

        return cameras_.Get(entity);
    }

    uint32_t CameraSystem::Count() const
    {
        return cameras_.Count();
    }

    bool CameraSystem::SetPerspective(
        EntityHandle entity,
        float fovYRadians,
        float aspect,
        float nearZ,
        float farZ)
    {
        if (!IsUsableEntity_(entity)
            || !IsValidPerspective_(fovYRadians, aspect, nearZ, farZ))
        {
            return false;
        }

        CameraComponent* camera = cameras_.Get(entity);
        if (!camera)
            return false;

        camera->fovYRadians = fovYRadians;
        camera->aspect = aspect;
        camera->nearZ = nearZ;
        camera->farZ = farZ;
        return true;
    }

    bool CameraSystem::SetAspect(EntityHandle entity, float aspect)
    {
        if (!IsUsableEntity_(entity) || aspect <= 0.0f)
            return false;

        CameraComponent* camera = cameras_.Get(entity);
        if (!camera)
            return false;

        camera->aspect = aspect;
        return true;
    }

    bool CameraSystem::SetEnabled(EntityHandle entity, bool enabled)
    {
        if (!IsUsableEntity_(entity))
            return false;

        CameraComponent* camera = cameras_.Get(entity);
        if (!camera)
            return false;

        camera->enabled = enabled;

        if (!enabled && activeCamera_ == entity)
            activeCamera_ = EntityHandle::Invalid();

        return true;
    }

    bool CameraSystem::SetActive(EntityHandle entity)
    {
        if (!IsUsableEntity_(entity)
            || !cameras_.Has(entity)
            || !HasUsableTransform_(entity))
        {
            return false;
        }

        const CameraComponent* camera = cameras_.Get(entity);
        if (!camera || !camera->enabled)
            return false;

        activeCamera_ = entity;
        return true;
    }

    void CameraSystem::ClearActive()
    {
        activeCamera_ = EntityHandle::Invalid();
    }

    EntityHandle CameraSystem::ActiveCamera() const
    {
        if (!activeCamera_.IsValid()
            || !IsUsableEntity_(activeCamera_)
            || !cameras_.Has(activeCamera_)
            || !HasUsableTransform_(activeCamera_))
        {
            return EntityHandle::Invalid();
        }

        const CameraComponent* camera = cameras_.Get(activeCamera_);
        if (!camera || !camera->enabled)
            return EntityHandle::Invalid();

        return activeCamera_;
    }

    bool CameraSystem::Rebuild(EntityHandle entity)
    {
        if (!IsUsableEntity_(entity) || !HasUsableTransform_(entity))
            return false;

        CameraComponent* camera = cameras_.Get(entity);
        if (!camera || !camera->enabled)
            return false;

        Mat4 world{};
        if (!transforms_->GetWorldMatrix(entity, world))
            return false;

        const Vec3 eye{
            world.m[12],
            world.m[13],
            world.m[14]
        };

        const Vec3 worldUpRaw{
            world.m[4],
            world.m[5],
            world.m[6]
        };

        const Vec3 worldForwardRaw{
            world.m[8],
            world.m[9],
            world.m[10]
        };

        if (LengthSq(worldUpRaw) <= kMinimumBasisLengthSq
            || LengthSq(worldForwardRaw) <= kMinimumBasisLengthSq)
        {
            return false;
        }

        const Vec3 forward = Normalize(worldForwardRaw);
        const Vec3 right = Normalize(Cross(worldUpRaw, forward));

        if (LengthSq(right) <= kMinimumBasisLengthSq)
            return false;

        const Vec3 up = Cross(forward, right);

        camera->view = LookToLH(eye, forward, up);
        camera->proj = PerspectiveFovLH(
            camera->fovYRadians,
            camera->aspect,
            camera->nearZ,
            camera->farZ);
        camera->viewProj = Mul(camera->proj, camera->view);
        return true;
    }

    bool CameraSystem::RebuildActive(
        Mat4& outView,
        Mat4& outProjection,
        Mat4& outViewProjection)
    {
        const EntityHandle active = ActiveCamera();
        if (!active.IsValid() || !Rebuild(active))
            return false;

        const CameraComponent* camera = cameras_.Get(active);
        if (!camera)
            return false;

        outView = camera->view;
        outProjection = camera->proj;
        outViewProjection = camera->viewProj;
        return true;
    }

    uint32_t CameraSystem::DenseCount() const
    {
        return cameras_.Count();
    }

    EntityHandle CameraSystem::OwnerAtDenseIndex(uint32_t denseIndex) const
    {
        return cameras_.OwnerAtDenseIndex(denseIndex);
    }

    const CameraComponent* CameraSystem::ComponentAtDenseIndex(
        uint32_t denseIndex) const
    {
        return cameras_.ComponentAtDenseIndex(denseIndex);
    }

    bool CameraSystem::IsUsableEntity_(EntityHandle entity) const
    {
        return entities_ && entities_->IsAlive(entity);
    }

    bool CameraSystem::HasUsableTransform_(EntityHandle entity) const
    {
        return transforms_ && transforms_->Has(entity);
    }

    bool CameraSystem::IsValidPerspective_(
        float fovYRadians,
        float aspect,
        float nearZ,
        float farZ)
    {
        return fovYRadians > 0.0f
            && fovYRadians < kPi
            && aspect > 0.0f
            && nearZ > 0.0f
            && farZ > nearZ;
    }
}
