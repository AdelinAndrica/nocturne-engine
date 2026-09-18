#pragma once

#include "EditorCommands.h"
#include "EditorSession.h"
#include "EditorTransformMath.h"
#include "Runtime/World.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <new>

namespace nocturne::editor
{
    enum class EditorGizmoTerminationReason : uint8_t
    {
        PointerRelease = 0,
        CaptureLost,
        FocusLost,
        Escape,
        ToolChanged,
        ContextInvalid,
        Shutdown
    };

    enum class EditorGizmoTerminationAction : uint8_t
    {
        Commit = 0,
        Cancel
    };

    // Design choice (not directly from the book): pointer release, capture loss,
    // and focus loss commit the live preview; explicit Escape/tool changes,
    // invalidated drag context, and shutdown cancel it.
    [[nodiscard]] constexpr EditorGizmoTerminationAction
    EditorGizmoTerminationActionFor(
        EditorGizmoTerminationReason reason) noexcept
    {
        switch (reason)
        {
        case EditorGizmoTerminationReason::PointerRelease:
        case EditorGizmoTerminationReason::CaptureLost:
        case EditorGizmoTerminationReason::FocusLost:
            return EditorGizmoTerminationAction::Commit;
        case EditorGizmoTerminationReason::Escape:
        case EditorGizmoTerminationReason::ToolChanged:
        case EditorGizmoTerminationReason::ContextInvalid:
        case EditorGizmoTerminationReason::Shutdown:
            return EditorGizmoTerminationAction::Cancel;
        }
        return EditorGizmoTerminationAction::Cancel;
    }

    enum class EditorGizmoCommitResult : uint8_t
    {
        NoActiveDrag = 0,
        NoChange,
        Committed,
        StaleTarget,
        Failed
    };

    [[nodiscard]] inline bool EditorIsTransformTool(
        EditorTool tool) noexcept
    {
        return tool == EditorTool::Move
            || tool == EditorTool::Rotate
            || tool == EditorTool::Scale;
    }

    [[nodiscard]] inline TransformOrientation
    EditorEffectiveGizmoOrientation(
        EditorTool tool,
        TransformOrientation requested) noexcept
    {
        return tool == EditorTool::Scale
            ? TransformOrientation::Local
            : requested;
    }

    [[nodiscard]] inline noc::Vec3
    EditorGizmoMatrixTranslation(
        const noc::Mat4& matrix) noexcept
    {
        return {
            noc::M(matrix, 0, 3),
            noc::M(matrix, 1, 3),
            noc::M(matrix, 2, 3)
        };
    }

    [[nodiscard]] inline noc::Vec3
    EditorGizmoMatrixColumn(
        const noc::Mat4& matrix,
        int column) noexcept
    {
        return {
            noc::M(matrix, 0, column),
            noc::M(matrix, 1, column),
            noc::M(matrix, 2, column)
        };
    }

    [[nodiscard]] inline noc::Quat
    EditorGizmoAxisAngle(
        const noc::Vec3& axis,
        float angle) noexcept
    {
        const noc::Vec3 normalized =
            noc::Normalize(axis);
        const float half = angle * 0.5f;
        const float s = std::sin(half);

        return EditorNormalizeQuaternion({
            normalized.x * s,
            normalized.y * s,
            normalized.z * s,
            std::cos(half)
        });
    }

    [[nodiscard]] inline bool EditorBuildGizmoFrame(
        noc::World& world,
        noc::EntityHandle entity,
        bool worldOrientation,
        noc::Vec3& outPivot,
        noc::Vec3 outAxes[3]) noexcept
    {
        if (!outAxes
            || !entity.IsValid()
            || !world.IsAlive(entity)
            || !world.HasTransform(entity))
        {
            return false;
        }

        const noc::Mat4 worldMatrix =
            world.GetWorldMatrix(entity);

        outPivot =
            EditorGizmoMatrixTranslation(worldMatrix);

        if (worldOrientation)
        {
            outAxes[0] = { 1.0f, 0.0f, 0.0f };
            outAxes[1] = { 0.0f, 1.0f, 0.0f };
            outAxes[2] = { 0.0f, 0.0f, 1.0f };
            return true;
        }

        for (int axis = 0; axis < 3; ++axis)
        {
            const noc::Vec3 column =
                EditorGizmoMatrixColumn(
                    worldMatrix,
                    axis);

            if (!noc::IsFiniteMath(column)
                || noc::LengthSq(column)
                    <= 1.0e-10f)
            {
                return false;
            }

            outAxes[axis] =
                noc::Normalize(column);
        }

        return true;
    }

    [[nodiscard]] inline bool EditorGizmoWorldToLocalTRS(
        noc::World& world,
        noc::EntityHandle entity,
        const noc::Mat4& desiredWorld,
        noc::Vec3& outTranslation,
        noc::Quat& outRotation,
        noc::Vec3& outScale) noexcept
    {
        if (!entity.IsValid()
            || !world.IsAlive(entity)
            || !world.HasTransform(entity))
        {
            return false;
        }

        noc::Mat4 desiredLocal =
            desiredWorld;

        const noc::EntityHandle parent =
            world.ParentOf(entity);

        if (parent.IsValid())
        {
            const noc::Mat4 parentWorld =
                world.GetWorldMatrix(parent);
            noc::Mat4 inverseParent{};

            if (!noc::TryInverseAffine(
                    parentWorld,
                    inverseParent))
            {
                return false;
            }

            desiredLocal =
                noc::Mul(
                    inverseParent,
                    desiredWorld);
        }

        return noc::TryDecomposeTRS(
            desiredLocal,
            outTranslation,
            outRotation,
            outScale);
    }

    class EditorGizmoDragTransaction final
    {
    public:
        [[nodiscard]] bool Begin(
            EditorCommandContext& context,
            noc::EntityHandle entity,
            EditorTool tool,
            TransformOrientation requestedOrientation,
            int axis) noexcept
        {
            if (active_
                || axis < 0
                || axis > 2
                || !EditorIsTransformTool(tool)
                || !context.world.IsAlive(entity)
                || context.IsToolOwned(entity)
                || !context.world.HasTransform(entity))
            {
                return false;
            }

            const noc::TransformComponent* transform =
                context.world.GetTransform(entity);
            if (!transform)
                return false;

            orientation_ =
                EditorEffectiveGizmoOrientation(
                    tool,
                    requestedOrientation);

            noc::Vec3 pivot{};
            noc::Vec3 axes[3]{};
            if (!EditorBuildGizmoFrame(
                    context.world,
                    entity,
                    orientation_
                        == TransformOrientation::World,
                    pivot,
                    axes))
            {
                return false;
            }

            entity_ = entity;
            parent_ = context.world.ParentOf(entity);
            tool_ = tool;
            axis_ = axis;
            startTranslation_ =
                transform->localTranslation;
            startRotation_ =
                transform->localRotation;
            startScale_ =
                transform->localScale;
            startWorld_ =
                context.world.GetWorldMatrix(entity);
            pivot_ = pivot;
            axes_[0] = axes[0];
            axes_[1] = axes[1];
            axes_[2] = axes[2];
            active_ = true;
            return true;
        }

        [[nodiscard]] bool IsActive() const noexcept { return active_; }
        [[nodiscard]] noc::EntityHandle Entity() const noexcept { return entity_; }
        [[nodiscard]] EditorTool Tool() const noexcept { return tool_; }
        [[nodiscard]] TransformOrientation Orientation() const noexcept { return orientation_; }
        [[nodiscard]] int AxisIndex() const noexcept { return axis_; }
        [[nodiscard]] const noc::Vec3& Pivot() const noexcept { return pivot_; }
        [[nodiscard]] const noc::Vec3& Axis(int index) const noexcept { return axes_[index]; }

        [[nodiscard]] bool InteractionModeMatches(
            EditorTool currentTool,
            TransformOrientation currentOrientation) const noexcept
        {
            return active_
                && currentTool == tool_
                && EditorEffectiveGizmoOrientation(
                    currentTool,
                    currentOrientation)
                    == orientation_;
        }

        [[nodiscard]] bool TargetStillValid(
            EditorCommandContext& context) const noexcept
        {
            return active_
                && entity_.IsValid()
                && context.world.IsAlive(entity_)
                && !context.IsToolOwned(entity_)
                && context.world.HasTransform(entity_)
                && context.world.ParentOf(entity_)
                    == parent_;
        }

        [[nodiscard]] bool PreviewMove(
            EditorCommandContext& context,
            float worldDelta) noexcept
        {
            if (tool_ != EditorTool::Move
                || !std::isfinite(worldDelta)
                || !TargetStillValid(context))
            {
                return false;
            }

            noc::Mat4 desiredWorld =
                startWorld_;

            const noc::Vec3 desiredPosition =
                pivot_
                + axes_[axis_] * worldDelta;

            noc::M(desiredWorld, 0, 3) = desiredPosition.x;
            noc::M(desiredWorld, 1, 3) = desiredPosition.y;
            noc::M(desiredWorld, 2, 3) = desiredPosition.z;

            noc::Vec3 translation{};
            noc::Quat rotation{};
            noc::Vec3 scale{};

            if (!EditorGizmoWorldToLocalTRS(
                    context.world,
                    entity_,
                    desiredWorld,
                    translation,
                    rotation,
                    scale))
            {
                return false;
            }

            return ApplyPreview_(
                context,
                translation,
                rotation,
                scale);
        }

        [[nodiscard]] bool PreviewRotate(
            EditorCommandContext& context,
            float angleRadians) noexcept
        {
            if (tool_ != EditorTool::Rotate
                || !std::isfinite(angleRadians)
                || !TargetStillValid(context))
            {
                return false;
            }

            noc::Vec3 translation =
                startTranslation_;
            noc::Quat rotation =
                startRotation_;
            noc::Vec3 scale =
                startScale_;

            if (orientation_
                == TransformOrientation::World)
            {
                const noc::Quat delta =
                    EditorGizmoAxisAngle(
                        axes_[axis_],
                        angleRadians);

                noc::Mat4 desiredWorld =
                    startWorld_;

                for (int column = 0;
                     column < 3;
                     ++column)
                {
                    const noc::Vec3 rotated =
                        noc::Rotate(
                            delta,
                            EditorGizmoMatrixColumn(
                                startWorld_,
                                column));

                    noc::M(desiredWorld, 0, column) = rotated.x;
                    noc::M(desiredWorld, 1, column) = rotated.y;
                    noc::M(desiredWorld, 2, column) = rotated.z;
                }

                if (!EditorGizmoWorldToLocalTRS(
                        context.world,
                        entity_,
                        desiredWorld,
                        translation,
                        rotation,
                        scale))
                {
                    return false;
                }
            }
            else
            {
                const noc::Vec3 unit[3] = {
                    { 1.0f, 0.0f, 0.0f },
                    { 0.0f, 1.0f, 0.0f },
                    { 0.0f, 0.0f, 1.0f }
                };

                rotation =
                    EditorNormalizeQuaternion(
                        EditorMultiplyQuaternions(
                            startRotation_,
                            EditorGizmoAxisAngle(
                                unit[axis_],
                                angleRadians)));
            }

            return ApplyPreview_(
                context,
                translation,
                rotation,
                scale);
        }

        [[nodiscard]] bool PreviewScale(
            EditorCommandContext& context,
            float normalizedDelta) noexcept
        {
            if (tool_ != EditorTool::Scale
                || orientation_
                    != TransformOrientation::Local
                || !std::isfinite(normalizedDelta)
                || !TargetStillValid(context))
            {
                return false;
            }

            noc::Vec3 scale =
                startScale_;

            float* component =
                axis_ == 0
                    ? &scale.x
                    : (axis_ == 1
                        ? &scale.y
                        : &scale.z);

            const float start =
                axis_ == 0
                    ? startScale_.x
                    : (axis_ == 1
                        ? startScale_.y
                        : startScale_.z);

            const float reference =
                (std::max)(
                    std::fabs(start),
                    0.25f);

            *component =
                (std::max)(
                    0.05f,
                    start
                        + normalizedDelta
                            * reference);

            return ApplyPreview_(
                context,
                startTranslation_,
                startRotation_,
                scale);
        }

        [[nodiscard]] bool Cancel(
            EditorCommandContext& context) noexcept
        {
            if (!active_)
                return false;

            bool restored = true;

            if (entity_.IsValid()
                && context.world.IsAlive(entity_)
                && context.world.HasTransform(entity_))
            {
                if (context.world.ParentOf(entity_)
                    != parent_)
                {
                    restored = false;
                }
                else
                {
                    restored =
                        context.world.SetLocalTRS(
                            entity_,
                            startTranslation_,
                            startRotation_,
                            startScale_);
                    if (restored)
                        context.world.Update();
                }
            }

            Reset();
            return restored;
        }

        [[nodiscard]] EditorGizmoCommitResult Commit(
            EditorSession& session)
        {
            if (!active_)
                return EditorGizmoCommitResult::NoActiveDrag;

            if (!session.IsInitialized())
            {
                Reset();
                return EditorGizmoCommitResult::Failed;
            }

            EditorCommandContext context =
                session.CommandContext();

            if (!TargetStillValid(context))
            {
                Reset();
                return EditorGizmoCommitResult::StaleTarget;
            }

            const noc::TransformComponent* finalTransform =
                context.world.GetTransform(entity_);
            if (!finalTransform)
            {
                Reset();
                return EditorGizmoCommitResult::StaleTarget;
            }

            const bool changed =
                startTranslation_.x != finalTransform->localTranslation.x
                || startTranslation_.y != finalTransform->localTranslation.y
                || startTranslation_.z != finalTransform->localTranslation.z
                || startRotation_.x != finalTransform->localRotation.x
                || startRotation_.y != finalTransform->localRotation.y
                || startRotation_.z != finalTransform->localRotation.z
                || startRotation_.w != finalTransform->localRotation.w
                || startScale_.x != finalTransform->localScale.x
                || startScale_.y != finalTransform->localScale.y
                || startScale_.z != finalTransform->localScale.z;

            if (!changed)
            {
                Reset();
                return EditorGizmoCommitResult::NoChange;
            }

            try
            {
                auto command =
                    std::make_unique<
                        SetTransformTRSCommand>();

                if (!command->InitExplicit(
                        context,
                        entity_,
                        startTranslation_,
                        startRotation_,
                        startScale_,
                        finalTransform->localTranslation,
                        finalTransform->localRotation,
                        finalTransform->localScale))
                {
                    Reset();
                    return EditorGizmoCommitResult::Failed;
                }

                if (!session.History().RecordExecuted(
                        context,
                        std::move(command)))
                {
                    Reset();
                    return EditorGizmoCommitResult::Failed;
                }
            }
            catch (const std::bad_alloc&)
            {
                (void)context.world.SetLocalTRS(
                    entity_,
                    startTranslation_,
                    startRotation_,
                    startScale_);
                context.world.Update();
                Reset();
                return EditorGizmoCommitResult::Failed;
            }

            session.SetSceneDirty();
            Reset();
            return EditorGizmoCommitResult::Committed;
        }

        void Reset() noexcept
        {
            active_ = false;
            entity_ = noc::EntityHandle::Invalid();
            parent_ = noc::EntityHandle::Invalid();
            tool_ = EditorTool::Select;
            orientation_ = TransformOrientation::Local;
            axis_ = -1;
            startTranslation_ = {};
            startRotation_ = noc::Quat::Identity();
            startScale_ = noc::Vec3::One();
            startWorld_ = noc::Mat4::Identity();
            pivot_ = {};
            axes_[0] = { 1.0f, 0.0f, 0.0f };
            axes_[1] = { 0.0f, 1.0f, 0.0f };
            axes_[2] = { 0.0f, 0.0f, 1.0f };
        }

    private:
        [[nodiscard]] bool ApplyPreview_(
            EditorCommandContext& context,
            const noc::Vec3& translation,
            const noc::Quat& rotation,
            const noc::Vec3& scale) noexcept
        {
            if (!context.world.SetLocalTRS(
                    entity_,
                    translation,
                    rotation,
                    scale))
            {
                return false;
            }
            context.world.Update();
            return true;
        }

        bool active_ = false;
        noc::EntityHandle entity_{};
        noc::EntityHandle parent_{};
        EditorTool tool_ = EditorTool::Select;
        TransformOrientation orientation_ =
            TransformOrientation::Local;
        int axis_ = -1;
        noc::Vec3 startTranslation_{};
        noc::Quat startRotation_ =
            noc::Quat::Identity();
        noc::Vec3 startScale_ =
            noc::Vec3::One();
        noc::Mat4 startWorld_ =
            noc::Mat4::Identity();
        noc::Vec3 pivot_{};
        noc::Vec3 axes_[3]{
            { 1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f }
        };
    };
}
