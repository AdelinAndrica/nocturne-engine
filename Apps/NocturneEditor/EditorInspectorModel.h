#pragma once

#include "EditorCommandHistory.h"
#include "Runtime/Entity.h"
#include "Runtime/Reflection/ReflectionIds.h"
#include "Runtime/Reflection/ReflectedValue.h"
#include "Runtime/Reflection/ReflectionMetadata.h"

#include <string>
#include <vector>

namespace nocturne::editor
{
    struct InspectorPropertyView
    {
        noc::PropertyId propertyId{};
        noc::TypeId valueTypeId{};
        noc::TypeKind valueKind = noc::TypeKind::Invalid;
        noc::PropertyFlags flags = noc::PropertyFlags::None;
        std::string displayName;
        std::string displayValue;
        bool editable = false;

        // Presentation copy only. Set when reflection explicitly marks the
        // property as an angle whose declared runtime units are radians.
        bool displayAngleDegrees = false;
    };

    struct InspectorComponentView
    {
        noc::TypeId typeId{};
        std::string displayName;
        std::vector<InspectorPropertyView> properties;
        bool removable = false;
    };

    // Generic reflection-driven Inspector data model. It stores IDs and display
    // copies only; no component/property metadata pointer survives Refresh().
    class EditorInspectorModel final
    {
    public:
        [[nodiscard]] bool Refresh(
            EditorCommandContext& context,
            noc::EntityHandle entity);

        void Clear() noexcept;

        [[nodiscard]] noc::EntityHandle Entity() const noexcept;
        [[nodiscard]] const std::vector<InspectorComponentView>&
        Components() const noexcept;

        [[nodiscard]] const InspectorPropertyView* FindProperty(
            noc::TypeId componentTypeId,
            noc::PropertyId propertyId) const noexcept;

        // Generic reflection-backed property read for editor presentation
        // extensions. No component pointer or metadata pointer escapes this call.
        [[nodiscard]] bool ReadValue(
            EditorCommandContext& context,
            noc::EntityHandle entity,
            noc::TypeId componentTypeId,
            noc::PropertyId propertyId,
            noc::OwnedReflectedValue& outValue) const;

        // Reads a nested reflected leaf by PropertyId path. Each hop is copied
        // through reflection lifecycle/read callbacks; no interior pointer is
        // retained beyond the call.
        [[nodiscard]] bool ReadNestedValue(
            EditorCommandContext& context,
            noc::EntityHandle entity,
            noc::TypeId componentTypeId,
            noc::PropertyId propertyId,
            const noc::PropertyId* nestedPath,
            uint32_t nestedPathCount,
            noc::OwnedReflectedValue& outValue) const;

        // Read-modify-writes a nested reflected leaf inside a copy of the
        // top-level component property, then authors that complete parent value
        // through one SetReflectedPropertyCommand. This preserves semantic
        // top-level setters and one-step undo/redo.
        [[nodiscard]] bool CommitNestedTextEdit(
            EditorCommandContext& context,
            EditorCommandHistory& history,
            noc::EntityHandle entity,
            noc::TypeId componentTypeId,
            noc::PropertyId propertyId,
            const noc::PropertyId* nestedPath,
            uint32_t nestedPathCount,
            const char* utf8Text);

        // Parses generic editor text into the reflected value type and executes
        // the mutation through SetReflectedPropertyCommand + command history.
        [[nodiscard]] bool CommitTextEdit(
            EditorCommandContext& context,
            EditorCommandHistory& history,
            noc::EntityHandle entity,
            noc::TypeId componentTypeId,
            noc::PropertyId propertyId,
            const char* utf8Text);

    private:
        [[nodiscard]] bool FormatValue_(
            EditorCommandContext& context,
            const noc::TypeMetadata& valueType,
            const noc::OwnedReflectedValue& value,
            std::string& outText) const;

        noc::EntityHandle entity_{};
        std::vector<InspectorComponentView> components_;
    };
}
