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
        return InitPreset(
            name,
            EditorEntityCreateKind::Empty,
            parent);
    }

    bool CreateEntityCommand::InitPreset(
        const char* name,
        EditorEntityCreateKind kind,
        noc::EntityHandle parent,
        noc::ResourceHandle mesh,
        const noc::AABB& localBounds)
    {
        if (initialized_ || !name)
            return false;

        switch (kind)
        {
        case EditorEntityCreateKind::Empty:
        case EditorEntityCreateKind::StaticMesh:
        case EditorEntityCreateKind::Camera:
        case EditorEntityCreateKind::DirectionalLight:
        case EditorEntityCreateKind::PointLight:
        case EditorEntityCreateKind::SpotLight:
            break;
        default:
            return false;
        }

        name_ = name;
        kind_ = kind;
        mesh_ = mesh;
        localBounds_ = localBounds;
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
        switch (kind_)
        {
        case EditorEntityCreateKind::StaticMesh:       return "Create Static Mesh Entity";
        case EditorEntityCreateKind::Camera:           return "Create Camera";
        case EditorEntityCreateKind::DirectionalLight: return "Create Directional Light";
        case EditorEntityCreateKind::PointLight:       return "Create Point Light";
        case EditorEntityCreateKind::SpotLight:        return "Create Spot Light";
        case EditorEntityCreateKind::Empty:
        default:                                       return "Create Empty Entity";
        }
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
                || context.IsToolOwned(requestedParent_)
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

        bool presetReady = true;
        switch (kind_)
        {
        case EditorEntityCreateKind::Empty:
            break;
        case EditorEntityCreateKind::StaticMesh:
            presetReady = context.world.AddRenderable(
                created,
                mesh_,
                localBounds_);
            break;
        case EditorEntityCreateKind::Camera:
            presetReady = context.world.AddCamera(created);
            break;
        case EditorEntityCreateKind::DirectionalLight:
            presetReady = context.world.AddLight(
                created,
                noc::LightType::Directional);
            break;
        case EditorEntityCreateKind::PointLight:
            presetReady = context.world.AddLight(
                created,
                noc::LightType::Point);
            break;
        case EditorEntityCreateKind::SpotLight:
            presetReady = context.world.AddLight(
                created,
                noc::LightType::Spot);
            break;
        default:
            presetReady = false;
            break;
        }

        if (!presetReady)
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
            || !context.world.IsAlive(entity)
            || context.IsToolOwned(entity))
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

    bool ReparentEntityCommand::Init(
        EditorCommandContext& context,
        noc::EntityHandle child,
        noc::EntityHandle newParent)
    {
        if (child_.IsValid()
            || !context.world.IsAlive(child)
            || context.IsToolOwned(child)
            || !context.world.HasTransform(child)
            || child == newParent)
        {
            return false;
        }

        if (newParent.IsValid()
            && (!context.world.IsAlive(newParent)
                || context.IsToolOwned(newParent)
                || !context.world.HasTransform(newParent)))
        {
            return false;
        }

        noc::EntityHandle ancestor = newParent;
        while (ancestor.IsValid())
        {
            if (ancestor == child)
                return false;

            ancestor =
                context.world.ParentOf(ancestor);
        }

        const noc::EntityHandle oldParent =
            context.world.ParentOf(child);

        if (oldParent == newParent)
            return false;

        const noc::TransformComponent* oldTransform =
            context.world.GetTransform(child);
        if (!oldTransform)
            return false;

        oldTranslation_ = oldTransform->localTranslation;
        oldRotation_ = oldTransform->localRotation;
        oldScale_ = oldTransform->localScale;

        const noc::Mat4 childWorld =
            context.world.GetWorldMatrix(child);

        noc::Mat4 targetLocal = childWorld;

        if (newParent.IsValid())
        {
            const noc::Mat4 parentWorld =
                context.world.GetWorldMatrix(newParent);
            noc::Mat4 inverseParent{};

            if (!noc::TryInverseAffine(
                    parentWorld,
                    inverseParent))
            {
                return false;
            }

            targetLocal =
                noc::Mul(inverseParent, childWorld);
        }

        if (!noc::TryDecomposeTRS(
                targetLocal,
                newTranslation_,
                newRotation_,
                newScale_))
        {
            return false;
        }

        child_ = child;
        oldParent_ = oldParent;
        newParent_ = newParent;
        return true;
    }

    const char* ReparentEntityCommand::Label() const noexcept
    {
        return "Reparent Entity";
    }

    std::size_t ReparentEntityCommand::MemoryCostBytes() const noexcept
    {
        return sizeof(*this);
    }

    bool ReparentEntityCommand::Execute(
        EditorCommandContext& context)
    {
        return Apply_(
            context,
            newParent_,
            newTranslation_,
            newRotation_,
            newScale_,
            oldParent_,
            oldTranslation_,
            oldRotation_,
            oldScale_);
    }

    bool ReparentEntityCommand::Undo(
        EditorCommandContext& context)
    {
        return Apply_(
            context,
            oldParent_,
            oldTranslation_,
            oldRotation_,
            oldScale_,
            newParent_,
            newTranslation_,
            newRotation_,
            newScale_);
    }

    bool ReparentEntityCommand::Redo(
        EditorCommandContext& context)
    {
        return Execute(context);
    }

    bool ReparentEntityCommand::Apply_(
        EditorCommandContext& context,
        noc::EntityHandle parent,
        const noc::Vec3& translation,
        const noc::Quat& rotation,
        const noc::Vec3& scale,
        noc::EntityHandle rollbackParent,
        const noc::Vec3& rollbackTranslation,
        const noc::Quat& rollbackRotation,
        const noc::Vec3& rollbackScale)
    {
        if (!context.world.IsAlive(child_)
            || context.IsToolOwned(child_)
            || (parent.IsValid()
                && (!context.world.IsAlive(parent)
                    || context.IsToolOwned(parent))))
        {
            return false;
        }

        if (!context.world.SetParent(child_, parent))
            return false;

        if (!context.world.SetLocalTRS(
                child_,
                translation,
                rotation,
                scale))
        {
            (void)context.world.SetParent(
                child_,
                rollbackParent);
            (void)context.world.SetLocalTRS(
                child_,
                rollbackTranslation,
                rollbackRotation,
                rollbackScale);
            context.world.Update();
            return false;
        }

        context.world.Update();
        return true;
    }

    bool DeleteEntityCommand::Init(
        EditorCommandContext& context,
        noc::EntityHandle entity)
    {
        return !context.IsToolOwned(entity)
            && snapshot_.Capture(context, entity);
    }

    noc::EntityHandle DeleteEntityCommand::CurrentRoot() const noexcept
    {
        return snapshot_.CurrentRoot();
    }

    const char* DeleteEntityCommand::Label() const noexcept
    {
        return "Delete Entity";
    }

    std::size_t DeleteEntityCommand::MemoryCostBytes() const noexcept
    {
        return sizeof(*this) + snapshot_.MemoryCostBytes();
    }

    bool DeleteEntityCommand::Execute(EditorCommandContext& context)
    {
        return snapshot_.DestroyCurrent(context);
    }

    bool DeleteEntityCommand::Undo(EditorCommandContext& context)
    {
        return snapshot_.Instantiate(
            context,
            snapshot_.OriginalRootParent());
    }

    bool DeleteEntityCommand::Redo(EditorCommandContext& context)
    {
        return snapshot_.DestroyCurrent(context);
    }

    bool DuplicateEntityCommand::Init(
        EditorCommandContext& context,
        noc::EntityHandle entity)
    {
        if (context.IsToolOwned(entity)
            || !snapshot_.Capture(context, entity))
        {
            return false;
        }

        rootParent_ =
            snapshot_.OriginalRootParent();
        snapshot_.ForgetCurrentInstances();
        return true;
    }

    noc::EntityHandle DuplicateEntityCommand::CurrentRoot() const noexcept
    {
        return snapshot_.CurrentRoot();
    }

    const char* DuplicateEntityCommand::Label() const noexcept
    {
        return "Duplicate Entity";
    }

    std::size_t DuplicateEntityCommand::MemoryCostBytes() const noexcept
    {
        return sizeof(*this) + snapshot_.MemoryCostBytes();
    }

    bool DuplicateEntityCommand::Execute(EditorCommandContext& context)
    {
        return snapshot_.Instantiate(
            context,
            rootParent_);
    }

    bool DuplicateEntityCommand::Undo(EditorCommandContext& context)
    {
        return snapshot_.DestroyCurrent(context);
    }

    bool DuplicateEntityCommand::Redo(EditorCommandContext& context)
    {
        return snapshot_.Instantiate(
            context,
            rootParent_);
    }

    bool AddComponentCommand::Init(
        EditorCommandContext& context,
        noc::EntityHandle entity,
        noc::TypeId componentTypeId)
    {
        if (entity_.IsValid()
            || !context.world.IsAlive(entity)
            || context.IsToolOwned(entity))
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
            || !context.world.IsAlive(entity)
            || context.IsToolOwned(entity))
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

    bool SetTransformTRSCommand::InitExplicit(
        EditorCommandContext& context,
        noc::EntityHandle entity,
        const noc::Vec3& oldTranslation,
        const noc::Quat& oldRotation,
        const noc::Vec3& oldScale,
        const noc::Vec3& newTranslation,
        const noc::Quat& newRotation,
        const noc::Vec3& newScale)
    {
        if (entity_.IsValid()
            || !context.world.IsAlive(entity)
            || context.IsToolOwned(entity)
            || !context.world.HasTransform(entity)
            || !noc::IsFiniteMath(oldTranslation)
            || !noc::IsFiniteMath(oldRotation)
            || !noc::IsFiniteMath(oldScale)
            || !noc::IsFiniteMath(newTranslation)
            || !noc::IsFiniteMath(newRotation)
            || !noc::IsFiniteMath(newScale))
        {
            return false;
        }

        entity_ = entity;

        oldTranslation_ = oldTranslation;
        oldRotation_ = oldRotation;
        oldScale_ = oldScale;

        newTranslation_ = newTranslation;
        newRotation_ = newRotation;
        newScale_ = newScale;
        return true;
    }

    const char* SetTransformTRSCommand::Label() const noexcept
    {
        return "Transform Entity";
    }

    std::size_t SetTransformTRSCommand::MemoryCostBytes() const noexcept
    {
        return sizeof(*this);
    }

    bool SetTransformTRSCommand::Execute(
        EditorCommandContext& context)
    {
        return Apply_(
            context,
            newTranslation_,
            newRotation_,
            newScale_);
    }

    bool SetTransformTRSCommand::Undo(
        EditorCommandContext& context)
    {
        return Apply_(
            context,
            oldTranslation_,
            oldRotation_,
            oldScale_);
    }

    bool SetTransformTRSCommand::Redo(
        EditorCommandContext& context)
    {
        return Execute(context);
    }

    bool SetTransformTRSCommand::Apply_(
        EditorCommandContext& context,
        const noc::Vec3& translation,
        const noc::Quat& rotation,
        const noc::Vec3& scale)
    {
        if (!context.world.IsAlive(entity_)
            || context.IsToolOwned(entity_)
            || !context.world.HasTransform(entity_))
        {
            return false;
        }

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

    bool SetReflectedPropertyCommand::Init(
        EditorCommandContext& context,
        noc::EntityHandle entity,
        noc::TypeId componentTypeId,
        noc::PropertyId propertyId,
        noc::ReflectedConstValueView newValue)
    {
        if (entity_.IsValid()
            || !context.world.IsAlive(entity)
            || context.IsToolOwned(entity)
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
            noc::MakeComponentPropertyAccessContext(
                runtime,
                componentType->componentMetadata);

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
            || context.IsToolOwned(entity)
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
            noc::MakeComponentPropertyAccessContext(
                runtime,
                componentType->componentMetadata);

        return noc::WritePropertyValue(
            *property,
            propertyContext,
            value) == noc::PropertyAccessStatus::Success;
    }
}
