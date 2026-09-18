#include "EditorCommands.h"

#include "Runtime/Reflection/ComponentReflection.h"
#include "Runtime/Reflection/PropertyAccess.h"
#include "Runtime/Reflection/ReflectionRegistry.h"
#include "Runtime/World.h"

namespace nocturne::editor
{
    bool CreateEntityCommand::Init(
        const char* name,
        noc::EntityHandle parent)
    {
        if (initialized_ || !name)
            return false;

        name_ = name;
        requestedParent_ = parent;
        initialized_ = true;
        return true;
    }

    noc::EntityHandle CreateEntityCommand::CurrentEntity() const noexcept
    {
        return currentEntity_;
    }

    const char* CreateEntityCommand::Label() const noexcept
    {
        return "Create Entity";
    }

    std::size_t CreateEntityCommand::MemoryCostBytes() const noexcept
    {
        return sizeof(*this) + name_.capacity() + 1u;
    }

    bool CreateEntityCommand::Execute(EditorCommandContext& context)
    {
        if (!initialized_ || currentEntity_.IsValid())
            return false;

        return Create_(context);
    }

    bool CreateEntityCommand::Undo(EditorCommandContext& context)
    {
        if (!currentEntity_.IsValid()
            || !context.world.IsAlive(currentEntity_))
        {
            return false;
        }

        if (!context.world.DestroyEntity(currentEntity_))
            return false;

        currentEntity_ = noc::EntityHandle::Invalid();
        return true;
    }

    bool CreateEntityCommand::Redo(EditorCommandContext& context)
    {
        if (!initialized_ || currentEntity_.IsValid())
            return false;

        return Create_(context);
    }

    bool CreateEntityCommand::Create_(EditorCommandContext& context)
    {
        if (requestedParent_.IsValid()
            && (!context.world.IsAlive(requestedParent_)
                || !context.world.HasTransform(requestedParent_)))
        {
            return false;
        }

        const noc::EntityHandle created =
            context.world.CreateEntity();
        if (!created.IsValid())
            return false;

        if (!context.world.AddName(created, name_.c_str())
            || !context.world.AddTransform(created))
        {
            (void)context.world.DestroyEntity(created);
            return false;
        }

        if (requestedParent_.IsValid()
            && !context.world.SetParent(
                created,
                requestedParent_))
        {
            (void)context.world.DestroyEntity(created);
            return false;
        }

        currentEntity_ = created;
        return true;
    }

    bool RenameEntityCommand::Init(
        EditorCommandContext& context,
        noc::EntityHandle entity,
        const char* newName)
    {
        if (entity_.IsValid()
            || !newName
            || !context.world.IsAlive(entity))
        {
            return false;
        }

        const noc::NameComponent* name =
            context.world.GetName(entity);
        if (!name)
            return false;

        oldName_ = name->value;
        newName_ = newName;

        // Validate through the semantic runtime seam without retaining a
        // component pointer. Restore the old value immediately.
        if (!context.world.SetName(entity, newName_.c_str()))
            return false;

        if (!context.world.SetName(entity, oldName_.c_str()))
            return false;

        entity_ = entity;
        return true;
    }

    const char* RenameEntityCommand::Label() const noexcept
    {
        return "Rename Entity";
    }

    std::size_t RenameEntityCommand::MemoryCostBytes() const noexcept
    {
        return sizeof(*this)
            + oldName_.capacity() + 1u
            + newName_.capacity() + 1u;
    }

    bool RenameEntityCommand::Execute(EditorCommandContext& context)
    {
        return Apply_(context, newName_);
    }

    bool RenameEntityCommand::Undo(EditorCommandContext& context)
    {
        return Apply_(context, oldName_);
    }

    bool RenameEntityCommand::Redo(EditorCommandContext& context)
    {
        return Apply_(context, newName_);
    }

    bool RenameEntityCommand::Apply_(
        EditorCommandContext& context,
        const std::string& value)
    {
        return context.world.IsAlive(entity_)
            && context.world.HasName(entity_)
            && context.world.SetName(
                entity_,
                value.c_str());
    }

    bool AddComponentCommand::Init(
        EditorCommandContext& context,
        noc::EntityHandle entity,
        noc::TypeId componentTypeId)
    {
        if (entity_.IsValid()
            || !context.world.IsAlive(entity))
        {
            return false;
        }

        const noc::TypeMetadata* type =
            context.reflection.FindType(componentTypeId);

        if (!type
            || type->kind != noc::TypeKind::Component
            || !type->componentMetadata
            || !noc::HasFlag(
                type->componentMetadata->flags,
                noc::ComponentReflectionFlags::EditorAddable)
            || type->componentMetadata->has(
                context.world,
                entity))
        {
            return false;
        }

        entity_ = entity;
        componentTypeId_ = componentTypeId;
        return true;
    }

    const char* AddComponentCommand::Label() const noexcept
    {
        return "Add Component";
    }

    std::size_t AddComponentCommand::MemoryCostBytes() const noexcept
    {
        return sizeof(*this);
    }

    bool AddComponentCommand::Execute(EditorCommandContext& context)
    {
        return Add_(context);
    }

    bool AddComponentCommand::Undo(EditorCommandContext& context)
    {
        return Remove_(context);
    }

    bool AddComponentCommand::Redo(EditorCommandContext& context)
    {
        return Add_(context);
    }

    bool AddComponentCommand::Add_(EditorCommandContext& context)
    {
        const noc::TypeMetadata* type =
            context.reflection.FindType(componentTypeId_);

        return context.world.IsAlive(entity_)
            && type
            && type->componentMetadata
            && !type->componentMetadata->has(
                context.world,
                entity_)
            && type->componentMetadata->add(
                context.world,
                entity_);
    }

    bool AddComponentCommand::Remove_(EditorCommandContext& context)
    {
        const noc::TypeMetadata* type =
            context.reflection.FindType(componentTypeId_);

        return context.world.IsAlive(entity_)
            && type
            && type->componentMetadata
            && type->componentMetadata->has(
                context.world,
                entity_)
            && type->componentMetadata->remove(
                context.world,
                entity_);
    }

    bool RemoveComponentCommand::Init(
        EditorCommandContext& context,
        noc::EntityHandle entity,
        noc::TypeId componentTypeId)
    {
        if (entity_.IsValid()
            || !context.world.IsAlive(entity))
        {
            return false;
        }

        const noc::TypeMetadata* type =
            context.reflection.FindType(componentTypeId);

        if (!type
            || type->kind != noc::TypeKind::Component
            || !type->componentMetadata
            || !noc::HasFlag(
                type->componentMetadata->flags,
                noc::ComponentReflectionFlags::EditorRemovable)
            || noc::HasFlag(
                type->componentMetadata->flags,
                noc::ComponentReflectionFlags::Required)
            || !type->componentMetadata->has(
                context.world,
                entity)
            || !snapshot_.Capture(
                context,
                entity,
                componentTypeId))
        {
            return false;
        }

        entity_ = entity;
        componentTypeId_ = componentTypeId;
        return true;
    }

    const char* RemoveComponentCommand::Label() const noexcept
    {
        return "Remove Component";
    }

    std::size_t RemoveComponentCommand::MemoryCostBytes() const noexcept
    {
        return sizeof(*this) + snapshot_.MemoryCostBytes();
    }

    bool RemoveComponentCommand::Execute(EditorCommandContext& context)
    {
        return Remove_(context);
    }

    bool RemoveComponentCommand::Undo(EditorCommandContext& context)
    {
        const noc::TypeMetadata* type =
            context.reflection.FindType(componentTypeId_);

        if (!context.world.IsAlive(entity_)
            || !type
            || !type->componentMetadata
            || type->componentMetadata->has(
                context.world,
                entity_)
            || !type->componentMetadata->add(
                context.world,
                entity_))
        {
            return false;
        }

        if (!snapshot_.Restore(context, entity_))
        {
            (void)type->componentMetadata->remove(
                context.world,
                entity_);
            return false;
        }

        return true;
    }

    bool RemoveComponentCommand::Redo(EditorCommandContext& context)
    {
        return Remove_(context);
    }

    bool RemoveComponentCommand::Remove_(
        EditorCommandContext& context)
    {
        const noc::TypeMetadata* type =
            context.reflection.FindType(componentTypeId_);

        return context.world.IsAlive(entity_)
            && type
            && type->componentMetadata
            && type->componentMetadata->has(
                context.world,
                entity_)
            && type->componentMetadata->remove(
                context.world,
                entity_);
    }

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

    bool SetReflectedPropertyCommand::InitExplicit(
        EditorCommandContext& context,
        noc::EntityHandle entity,
        noc::TypeId componentTypeId,
        noc::PropertyId propertyId,
        noc::ReflectedConstValueView oldValue,
        noc::ReflectedConstValueView newValue)
    {
        if (entity_.IsValid()
            || !context.world.IsAlive(entity)
            || !oldValue.IsValid()
            || !newValue.IsValid()
            || oldValue.typeId != newValue.typeId)
        {
            return false;
        }

        const noc::TypeMetadata* componentType =
            context.reflection.FindType(componentTypeId);
        const noc::PropertyMetadata* property =
            context.reflection.FindProperty(
                componentTypeId,
                propertyId);
        const noc::TypeMetadata* valueType =
            context.reflection.FindType(newValue.typeId);

        if (!componentType
            || componentType->kind != noc::TypeKind::Component
            || !componentType->componentMetadata
            || !property
            || !valueType
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

        if (!oldValue_.InitCopy(
                context.allocator,
                *valueType,
                oldValue.data))
        {
            return false;
        }

        if (!newValue_.InitCopy(
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
