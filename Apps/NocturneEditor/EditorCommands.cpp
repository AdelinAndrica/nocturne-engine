#include "EditorCommands.h"

#include "Runtime/Reflection/ComponentReflection.h"
#include "Runtime/Reflection/PropertyAccess.h"
#include "Runtime/Reflection/ReflectionRegistry.h"
#include "Runtime/World.h"

namespace nocturne::editor
{
    bool SetReflectedPropertyCommand::Init(
        EditorCommandContext& context,
        noc::EntityHandle entity,
        noc::TypeId componentTypeId,
        noc::PropertyId propertyId,
        noc::ReflectedConstValueView newValue)
    {
        if (entity_.IsValid()
            || !context.world.IsAlive(entity)
            || !newValue.IsValid())
        {
            return false;
        }

        const noc::TypeMetadata* componentType =
            context.reflection.FindType(componentTypeId);
        const noc::PropertyMetadata* property =
            context.reflection.FindProperty(
                componentTypeId,
                propertyId);

        if (!componentType
            || componentType->kind != noc::TypeKind::Component
            || !componentType->componentMetadata
            || !property
            || property->valueTypeId != newValue.typeId
            || noc::HasFlag(
                property->flags,
                noc::PropertyFlags::ReadOnly)
            || !componentType->componentMetadata->has(
                context.world,
                entity))
        {
            return false;
        }

        noc::ComponentPropertyRuntimeContext runtime{
            &context.world,
            entity
        };
        noc::PropertyAccessContext propertyContext =
            noc::MakeComponentPropertyAccessContext(runtime);

        if (noc::ReadPropertyValue(
                context.reflection,
                *property,
                propertyContext,
                context.allocator,
                oldValue_)
            != noc::PropertyAccessStatus::Success)
        {
            return false;
        }

        const noc::TypeMetadata* valueType =
            context.reflection.FindType(newValue.typeId);
        if (!valueType
            || !newValue_.InitCopy(
                context.allocator,
                *valueType,
                newValue.data))
        {
            oldValue_.Clear();
            return false;
        }

        entity_ = entity;
        componentTypeId_ = componentTypeId;
        propertyId_ = propertyId;
        return true;
    }

    const char* SetReflectedPropertyCommand::Label() const noexcept
    {
        return "Set Property";
    }

    std::size_t SetReflectedPropertyCommand::MemoryCostBytes() const noexcept
    {
        return sizeof(*this)
            + oldValue_.Size()
            + newValue_.Size();
    }

    bool SetReflectedPropertyCommand::Execute(
        EditorCommandContext& context)
    {
        return Apply_(context, newValue_.ConstView());
    }

    bool SetReflectedPropertyCommand::Undo(
        EditorCommandContext& context)
    {
        return Apply_(context, oldValue_.ConstView());
    }

    bool SetReflectedPropertyCommand::Redo(
        EditorCommandContext& context)
    {
        return Apply_(context, newValue_.ConstView());
    }

    bool SetReflectedPropertyCommand::Apply_(
        EditorCommandContext& context,
        noc::ReflectedConstValueView value)
    {
        if (!context.world.IsAlive(entity_))
            return false;

        const noc::TypeMetadata* componentType =
            context.reflection.FindType(componentTypeId_);
        const noc::PropertyMetadata* property =
            context.reflection.FindProperty(
                componentTypeId_,
                propertyId_);

        if (!componentType
            || componentType->kind != noc::TypeKind::Component
            || !componentType->componentMetadata
            || !property
            || !componentType->componentMetadata->has(
                context.world,
                entity_))
        {
            return false;
        }

        noc::ComponentPropertyRuntimeContext runtime{
            &context.world,
            entity_
        };
        noc::PropertyAccessContext propertyContext =
            noc::MakeComponentPropertyAccessContext(runtime);

        return noc::WritePropertyValue(
            *property,
            propertyContext,
            value) == noc::PropertyAccessStatus::Success;
    }
}
