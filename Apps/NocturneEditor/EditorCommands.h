#pragma once

#include "EditorCommandHistory.h"
#include "Core/Math/MathTypes.h"
#include "EditorReflectionSnapshot.h"
#include "EditorEntitySnapshot.h"
#include "Runtime/Entity.h"
#include "Runtime/Bounds.h"
#include "Resources/ResourceHandle.h"
#include "Runtime/Reflection/ReflectedValue.h"
#include "Runtime/Reflection/ReflectionIds.h"

#include <cstddef>
#include <string>

namespace nocturne::editor
{
    // Design choice (not directly from the book): these are authoring presets,
    // not runtime entity subclasses or a second entity type system. They only
    // describe the initial component composition used by CreateEntityCommand.
    enum class EditorEntityCreateKind : uint8_t
    {
        Empty = 0,
        StaticMesh,
        Camera,
        DirectionalLight,
        PointLight,
        SpotLight
    };

    // Design choice (not directly from the book): editor-created authored
    // entities begin with Name + Transform. Runtime World itself keeps generic
    // entity creation component-free.
    class CreateEntityCommand final : public IEditorCommand
    {
    public:
        [[nodiscard]] bool Init(
            const char* name,
            noc::EntityHandle parent = noc::EntityHandle::Invalid());

        [[nodiscard]] bool InitPreset(
            const char* name,
            EditorEntityCreateKind kind,
            noc::EntityHandle parent = noc::EntityHandle::Invalid(),
            noc::ResourceHandle mesh = {},
            const noc::AABB& localBounds = {});

        [[nodiscard]] noc::EntityHandle CurrentEntity() const noexcept;

        [[nodiscard]] const char* Label() const noexcept override;
        [[nodiscard]] std::size_t MemoryCostBytes() const noexcept override;

        [[nodiscard]] bool Execute(EditorCommandContext& context) override;
        [[nodiscard]] bool Undo(EditorCommandContext& context) override;
        [[nodiscard]] bool Redo(EditorCommandContext& context) override;

        [[nodiscard]] noc::EntityHandle
        SelectionHintAfterExecute() const noexcept override
        {
            return currentEntity_;
        }

        [[nodiscard]] noc::EntityHandle
        SelectionHintAfterRedo() const noexcept override
        {
            return currentEntity_;
        }

    private:
        [[nodiscard]] bool Create_(EditorCommandContext& context);

        std::string name_;
        EditorEntityCreateKind kind_ = EditorEntityCreateKind::Empty;
        noc::ResourceHandle mesh_{};
        noc::AABB localBounds_{};
        noc::EntityHandle requestedParent_{};
        noc::EntityHandle currentEntity_{};
        bool initialized_ = false;
    };

    class RenameEntityCommand final : public IEditorCommand
    {
    public:
        [[nodiscard]] bool Init(
            EditorCommandContext& context,
            noc::EntityHandle entity,
            const char* newName);

        [[nodiscard]] const char* Label() const noexcept override;
        [[nodiscard]] std::size_t MemoryCostBytes() const noexcept override;

        [[nodiscard]] bool Execute(EditorCommandContext& context) override;
        [[nodiscard]] bool Undo(EditorCommandContext& context) override;
        [[nodiscard]] bool Redo(EditorCommandContext& context) override;

    private:
        [[nodiscard]] bool Apply_(
            EditorCommandContext& context,
            const std::string& value);

        noc::EntityHandle entity_{};
        std::string oldName_;
        std::string newName_;
    };

    class ReparentEntityCommand final : public IEditorCommand
    {
    public:
        [[nodiscard]] bool Init(
            EditorCommandContext& context,
            noc::EntityHandle child,
            noc::EntityHandle newParent);

        [[nodiscard]] const char* Label() const noexcept override;
        [[nodiscard]] std::size_t MemoryCostBytes() const noexcept override;

        [[nodiscard]] bool Execute(EditorCommandContext& context) override;
        [[nodiscard]] bool Undo(EditorCommandContext& context) override;
        [[nodiscard]] bool Redo(EditorCommandContext& context) override;

    private:
        [[nodiscard]] bool Apply_(
            EditorCommandContext& context,
            noc::EntityHandle parent,
            const noc::Vec3& translation,
            const noc::Quat& rotation,
            const noc::Vec3& scale,
            noc::EntityHandle rollbackParent,
            const noc::Vec3& rollbackTranslation,
            const noc::Quat& rollbackRotation,
            const noc::Vec3& rollbackScale);

        noc::EntityHandle child_{};
        noc::EntityHandle oldParent_{};
        noc::EntityHandle newParent_{};

        noc::Vec3 oldTranslation_{};
        noc::Quat oldRotation_ = noc::Quat::Identity();
        noc::Vec3 oldScale_ = noc::Vec3::One();

        noc::Vec3 newTranslation_{};
        noc::Quat newRotation_ = noc::Quat::Identity();
        noc::Vec3 newScale_ = noc::Vec3::One();
    };

    class DeleteEntityCommand final : public IEditorCommand
    {
    public:
        [[nodiscard]] bool Init(
            EditorCommandContext& context,
            noc::EntityHandle entity);

        [[nodiscard]] noc::EntityHandle CurrentRoot() const noexcept;

        [[nodiscard]] const char* Label() const noexcept override;
        [[nodiscard]] std::size_t MemoryCostBytes() const noexcept override;

        [[nodiscard]] bool Execute(EditorCommandContext& context) override;
        [[nodiscard]] bool Undo(EditorCommandContext& context) override;
        [[nodiscard]] bool Redo(EditorCommandContext& context) override;

        [[nodiscard]] noc::EntityHandle
        SelectionHintAfterUndo() const noexcept override
        {
            return snapshot_.CurrentRoot();
        }

    private:
        ReflectedEntitySubtreeSnapshot snapshot_;
    };

    class DuplicateEntityCommand final : public IEditorCommand
    {
    public:
        [[nodiscard]] bool Init(
            EditorCommandContext& context,
            noc::EntityHandle entity);

        [[nodiscard]] noc::EntityHandle CurrentRoot() const noexcept;

        [[nodiscard]] const char* Label() const noexcept override;
        [[nodiscard]] std::size_t MemoryCostBytes() const noexcept override;

        [[nodiscard]] bool Execute(EditorCommandContext& context) override;
        [[nodiscard]] bool Undo(EditorCommandContext& context) override;
        [[nodiscard]] bool Redo(EditorCommandContext& context) override;

        [[nodiscard]] noc::EntityHandle
        SelectionHintAfterExecute() const noexcept override
        {
            return snapshot_.CurrentRoot();
        }

        [[nodiscard]] noc::EntityHandle
        SelectionHintAfterRedo() const noexcept override
        {
            return snapshot_.CurrentRoot();
        }

    private:
        ReflectedEntitySubtreeSnapshot snapshot_;
        noc::EntityHandle rootParent_{};
    };

    class AddComponentCommand final : public IEditorCommand
    {
    public:
        [[nodiscard]] bool Init(
            EditorCommandContext& context,
            noc::EntityHandle entity,
            noc::TypeId componentTypeId);

        [[nodiscard]] const char* Label() const noexcept override;
        [[nodiscard]] std::size_t MemoryCostBytes() const noexcept override;

        [[nodiscard]] bool Execute(EditorCommandContext& context) override;
        [[nodiscard]] bool Undo(EditorCommandContext& context) override;
        [[nodiscard]] bool Redo(EditorCommandContext& context) override;

    private:
        [[nodiscard]] bool Add_(EditorCommandContext& context);
        [[nodiscard]] bool Remove_(EditorCommandContext& context);

        noc::EntityHandle entity_{};
        noc::TypeId componentTypeId_{};
    };

    class RemoveComponentCommand final : public IEditorCommand
    {
    public:
        [[nodiscard]] bool Init(
            EditorCommandContext& context,
            noc::EntityHandle entity,
            noc::TypeId componentTypeId);

        [[nodiscard]] const char* Label() const noexcept override;
        [[nodiscard]] std::size_t MemoryCostBytes() const noexcept override;

        [[nodiscard]] bool Execute(EditorCommandContext& context) override;
        [[nodiscard]] bool Undo(EditorCommandContext& context) override;
        [[nodiscard]] bool Redo(EditorCommandContext& context) override;

    private:
        [[nodiscard]] bool Remove_(EditorCommandContext& context);

        noc::EntityHandle entity_{};
        noc::TypeId componentTypeId_{};
        ReflectedComponentSnapshot snapshot_;
    };

    // Design choice (not directly from the book): gizmo/world-space authoring
    // records the full local TRS atomically because a world-space edit can
    // legitimately change more than one local field after parent-space
    // conversion and decomposition.
    class SetTransformTRSCommand final : public IEditorCommand
    {
    public:
        [[nodiscard]] bool InitExplicit(
            EditorCommandContext& context,
            noc::EntityHandle entity,
            const noc::Vec3& oldTranslation,
            const noc::Quat& oldRotation,
            const noc::Vec3& oldScale,
            const noc::Vec3& newTranslation,
            const noc::Quat& newRotation,
            const noc::Vec3& newScale);

        [[nodiscard]] const char* Label() const noexcept override;
        [[nodiscard]] std::size_t MemoryCostBytes() const noexcept override;

        [[nodiscard]] bool Execute(EditorCommandContext& context) override;
        [[nodiscard]] bool Undo(EditorCommandContext& context) override;
        [[nodiscard]] bool Redo(EditorCommandContext& context) override;

    private:
        [[nodiscard]] bool Apply_(
            EditorCommandContext& context,
            const noc::Vec3& translation,
            const noc::Quat& rotation,
            const noc::Vec3& scale);

        noc::EntityHandle entity_{};

        noc::Vec3 oldTranslation_{};
        noc::Quat oldRotation_ = noc::Quat::Identity();
        noc::Vec3 oldScale_ = noc::Vec3::One();

        noc::Vec3 newTranslation_{};
        noc::Quat newRotation_ = noc::Quat::Identity();
        noc::Vec3 newScale_ = noc::Vec3::One();
    };

    class SetReflectedPropertyCommand final : public IEditorCommand
    {
    public:
        SetReflectedPropertyCommand() = default;

        SetReflectedPropertyCommand(
            const SetReflectedPropertyCommand&) = delete;
        SetReflectedPropertyCommand& operator=(
            const SetReflectedPropertyCommand&) = delete;

        [[nodiscard]] bool Init(
            EditorCommandContext& context,
            noc::EntityHandle entity,
            noc::TypeId componentTypeId,
            noc::PropertyId propertyId,
            noc::ReflectedConstValueView newValue);

        // Used by live editor transactions (gizmo/text drag): both endpoints
        // are already known, so no component pointer or mutable baseline is
        // retained.
        [[nodiscard]] bool InitExplicit(
            EditorCommandContext& context,
            noc::EntityHandle entity,
            noc::TypeId componentTypeId,
            noc::PropertyId propertyId,
            noc::ReflectedConstValueView oldValue,
            noc::ReflectedConstValueView newValue);

        [[nodiscard]] const char* Label() const noexcept override;
        [[nodiscard]] std::size_t MemoryCostBytes() const noexcept override;

        [[nodiscard]] bool Execute(
            EditorCommandContext& context) override;
        [[nodiscard]] bool Undo(
            EditorCommandContext& context) override;
        [[nodiscard]] bool Redo(
            EditorCommandContext& context) override;

    private:
        [[nodiscard]] bool Apply_(
            EditorCommandContext& context,
            noc::ReflectedConstValueView value);

        noc::EntityHandle entity_{};
        noc::TypeId componentTypeId_{};
        noc::PropertyId propertyId_{};
        noc::OwnedReflectedValue oldValue_;
        noc::OwnedReflectedValue newValue_;
    };
}
