#pragma once

#include "EditorCommandHistory.h"
#include "Runtime/Entity.h"
#include "Runtime/Reflection/ReflectedValue.h"
#include "Runtime/Reflection/ReflectionIds.h"

#include <cstddef>

namespace nocturne::editor
{
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
