#pragma once

#include "EditorCommandHistory.h"
#include "EditorReflectionSnapshot.h"
#include "EditorEntitySnapshot.h"
#include "Runtime/Entity.h"
#include "Runtime/Reflection/ReflectedValue.h"
#include "Runtime/Reflection/ReflectionIds.h"

#include <cstddef>
#include <string>

namespace nocturne::editor
{
    // Design choice (not directly from the book): editor-created authored
    // entities begin with Name + Transform. Runtime World itself keeps generic
    // entity creation component-free.
    class CreateEntityCommand final : public IEditorCommand
    {
    public:
        [[nodiscard]] bool Init(
            const char* name,
            noc::EntityHandle parent = noc::EntityHandle::Invalid());

        [[nodiscard]] noc::EntityHandle CurrentEntity() const noexcept;

        [[nodiscard]] const char* Label() const noexcept override;
        [[nodiscard]] std::size_t MemoryCostBytes() const noexcept override;

        [[nodiscard]] bool Execute(EditorCommandContext& context) override;
        [[nodiscard]] bool Undo(EditorCommandContext& context) override;
        [[nodiscard]] bool Redo(EditorCommandContext& context) override;

    private:
        [[nodiscard]] bool Create_(EditorCommandContext& context);

        std::string name_;
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
