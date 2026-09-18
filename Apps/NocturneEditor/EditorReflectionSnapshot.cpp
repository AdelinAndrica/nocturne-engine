#include "EditorReflectionSnapshot.h"

#include "Runtime/Reflection/ComponentReflection.h"
#include "Runtime/Reflection/PropertyAccess.h"
#include "Runtime/Reflection/ReflectionRegistry.h"
#include "Runtime/World.h"

#include <new>

namespace nocturne::editor
{
    bool ReflectedComponentSnapshot::Capture(
        EditorCommandContext& context,
        noc::EntityHandle entity,
        noc::TypeId componentTypeId)
    {
        if (captured_
            || !context.world.IsAlive(entity))
        {
            return false;
        }

        const noc::TypeMetadata* type =
            context.reflection.FindType(componentTypeId);

        if (!type
            || type->kind != noc::TypeKind::Component
            || !type->componentMetadata
            || !type->componentMetadata->has(
                context.world,
                entity))
        {
            return false;
        }

        uint32_t captureCount = 0;
        for (uint32_t i = 0; i < type->propertyCount; ++i)
        {
            const noc::PropertyMetadata& property =
                type->properties[i];

            if (noc::HasFlag(
                    property.flags,
                    noc::PropertyFlags::Serializable)
                && !noc::HasFlag(
                    property.flags,
                    noc::PropertyFlags::Transient)
                && !noc::HasFlag(
                    property.flags,
                    noc::PropertyFlags::ReadOnly))
            {
                ++captureCount;
            }
        }

        try
        {
            properties_.reserve(captureCount);
        }
        catch (const std::bad_alloc&)
        {
            return false;
        }

        noc::ComponentPropertyRuntimeContext runtime{
            &context.world,
            entity
        };
        noc::PropertyAccessContext propertyContext =
            noc::MakeComponentPropertyAccessContext(runtime);

        for (uint32_t i = 0; i < type->propertyCount; ++i)
        {
            const noc::PropertyMetadata& property =
                type->properties[i];

            if (!noc::HasFlag(
                    property.flags,
                    noc::PropertyFlags::Serializable)
                || noc::HasFlag(
                    property.flags,
                    noc::PropertyFlags::Transient)
                || noc::HasFlag(
                    property.flags,
                    noc::PropertyFlags::ReadOnly))
            {
                continue;
            }

            PropertyEntry entry{};
            entry.propertyId = property.propertyId;

            if (noc::ReadPropertyValue(
                    context.reflection,
                    property,
                    propertyContext,
                    context.allocator,
                    entry.value)
                != noc::PropertyAccessStatus::Success)
            {
                Clear();
                return false;
            }

            try
            {
                properties_.push_back(std::move(entry));
            }
            catch (const std::bad_alloc&)
            {
                Clear();
                return false;
            }
        }

        componentTypeId_ = componentTypeId;
        captured_ = true;
        return true;
    }

    bool ReflectedComponentSnapshot::Restore(
        EditorCommandContext& context,
        noc::EntityHandle entity)
    {
        if (!captured_
            || !context.world.IsAlive(entity))
        {
            return false;
        }

        const noc::TypeMetadata* type =
            context.reflection.FindType(componentTypeId_);

        if (!type
            || type->kind != noc::TypeKind::Component
            || !type->componentMetadata
            || !type->componentMetadata->has(
                context.world,
                entity))
        {
            return false;
        }

        for (PropertyEntry& entry : properties_)
            entry.restored = false;

        noc::ComponentPropertyRuntimeContext runtime{
            &context.world,
            entity
        };
        noc::PropertyAccessContext propertyContext =
            noc::MakeComponentPropertyAccessContext(runtime);

        uint32_t remaining =
            static_cast<uint32_t>(properties_.size());

        for (uint32_t pass = 0;
             pass < properties_.size() && remaining > 0;
             ++pass)
        {
            bool progress = false;

            for (PropertyEntry& entry : properties_)
            {
                if (entry.restored)
                    continue;

                const noc::PropertyMetadata* property =
                    context.reflection.FindProperty(
                        componentTypeId_,
                        entry.propertyId);
                if (!property)
                    return false;

                if (noc::WritePropertyValue(
                        *property,
                        propertyContext,
                        entry.value.ConstView())
                    == noc::PropertyAccessStatus::Success)
                {
                    entry.restored = true;
                    --remaining;
                    progress = true;
                }
            }

            if (!progress)
                break;
        }

        return remaining == 0;
    }

    void ReflectedComponentSnapshot::Clear() noexcept
    {
        properties_.clear();
        componentTypeId_ = noc::TypeId::Invalid();
        captured_ = false;
    }

    bool ReflectedComponentSnapshot::IsValid() const noexcept
    {
        return captured_ && componentTypeId_.IsValid();
    }

    noc::TypeId ReflectedComponentSnapshot::ComponentType() const noexcept
    {
        return componentTypeId_;
    }

    std::size_t ReflectedComponentSnapshot::MemoryCostBytes() const noexcept
    {
        std::size_t result =
            sizeof(*this)
            + properties_.capacity() * sizeof(PropertyEntry);

        for (const PropertyEntry& entry : properties_)
            result += entry.value.Size();

        return result;
    }
}
