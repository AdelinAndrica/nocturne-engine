#include "Runtime/NameSystem.h"

#include "Runtime/EntityRegistry.h"

#include <cstring>

namespace noc
{
    namespace
    {
        [[nodiscard]] bool IsUtf8Continuation(unsigned char value)
        {
            return (value & 0xC0u) == 0x80u;
        }

        [[nodiscard]] bool IsValidUtf8(
            const char* text,
            uint32_t length)
        {
            uint32_t i = 0;

            while (i < length)
            {
                const unsigned char lead =
                    static_cast<unsigned char>(text[i]);

                if (lead <= 0x7Fu)
                {
                    ++i;
                    continue;
                }

                if (lead >= 0xC2u && lead <= 0xDFu)
                {
                    if (i + 1u >= length
                        || !IsUtf8Continuation(
                            static_cast<unsigned char>(text[i + 1u])))
                    {
                        return false;
                    }

                    i += 2u;
                    continue;
                }

                if (lead >= 0xE0u && lead <= 0xEFu)
                {
                    if (i + 2u >= length)
                        return false;

                    const unsigned char b1 =
                        static_cast<unsigned char>(text[i + 1u]);
                    const unsigned char b2 =
                        static_cast<unsigned char>(text[i + 2u]);

                    if (!IsUtf8Continuation(b2))
                        return false;

                    if (lead == 0xE0u)
                    {
                        if (b1 < 0xA0u || b1 > 0xBFu)
                            return false;
                    }
                    else if (lead == 0xEDu)
                    {
                        if (b1 < 0x80u || b1 > 0x9Fu)
                            return false;
                    }
                    else if (!IsUtf8Continuation(b1))
                    {
                        return false;
                    }

                    i += 3u;
                    continue;
                }

                if (lead >= 0xF0u && lead <= 0xF4u)
                {
                    if (i + 3u >= length)
                        return false;

                    const unsigned char b1 =
                        static_cast<unsigned char>(text[i + 1u]);
                    const unsigned char b2 =
                        static_cast<unsigned char>(text[i + 2u]);
                    const unsigned char b3 =
                        static_cast<unsigned char>(text[i + 3u]);

                    if (!IsUtf8Continuation(b2)
                        || !IsUtf8Continuation(b3))
                    {
                        return false;
                    }

                    if (lead == 0xF0u)
                    {
                        if (b1 < 0x90u || b1 > 0xBFu)
                            return false;
                    }
                    else if (lead == 0xF4u)
                    {
                        if (b1 < 0x80u || b1 > 0x8Fu)
                            return false;
                    }
                    else if (!IsUtf8Continuation(b1))
                    {
                        return false;
                    }

                    i += 4u;
                    continue;
                }

                return false;
            }

            return true;
        }
    }

    NameSystem::~NameSystem()
    {
        Shutdown();
    }

    bool NameSystem::Init(
        EntityRegistry& entities,
        IAllocator& allocator,
        uint32_t initialCapacity)
    {
        if (entities_)
            return true;

        const uint32_t sparseCapacity =
            entities.Capacity() > initialCapacity
                ? entities.Capacity()
                : initialCapacity;

        if (!names_.Init(
                allocator,
                initialCapacity,
                sparseCapacity))
        {
            return false;
        }

        entities_ = &entities;
        return true;
    }

    void NameSystem::Shutdown()
    {
        names_.Shutdown();
        entities_ = nullptr;
    }

    NameComponent* NameSystem::Add(
        EntityHandle entity,
        const char* initialName)
    {
        if (!IsUsableEntity_(entity))
            return nullptr;

        uint32_t length = 0;
        if (!ValidateName_(initialName, length))
            return nullptr;

        NameComponent* component = names_.Emplace(entity);
        if (!component)
            return nullptr;

        if (length > 0)
            std::memcpy(component->value, initialName, length);
        component->value[length] = '\0';
        return component;
    }

    bool NameSystem::Remove(EntityHandle entity)
    {
        if (!IsUsableEntity_(entity))
            return false;

        return names_.Remove(entity);
    }

    bool NameSystem::Has(EntityHandle entity) const
    {
        return IsUsableEntity_(entity) && names_.Has(entity);
    }

    const NameComponent* NameSystem::Get(EntityHandle entity) const
    {
        if (!IsUsableEntity_(entity))
            return nullptr;

        return names_.Get(entity);
    }

    uint32_t NameSystem::Count() const
    {
        return names_.Count();
    }

    bool NameSystem::SetName(EntityHandle entity, const char* name)
    {
        if (!IsUsableEntity_(entity))
            return false;

        uint32_t length = 0;
        if (!ValidateName_(name, length))
            return false;

        NameComponent* component = names_.Get(entity);
        if (!component)
            return false;

        if (length > 0)
            std::memcpy(component->value, name, length);
        component->value[length] = '\0';
        return true;
    }

    uint32_t NameSystem::DenseCount() const
    {
        return names_.Count();
    }

    EntityHandle NameSystem::OwnerAtDenseIndex(uint32_t denseIndex) const
    {
        return names_.OwnerAtDenseIndex(denseIndex);
    }

    const NameComponent* NameSystem::ComponentAtDenseIndex(
        uint32_t denseIndex) const
    {
        return names_.ComponentAtDenseIndex(denseIndex);
    }

    bool NameSystem::IsUsableEntity_(EntityHandle entity) const
    {
        return entities_ && entities_->IsAlive(entity);
    }

    bool NameSystem::ValidateName_(
        const char* name,
        uint32_t& outLength)
    {
        outLength = 0;
        if (!name)
            return false;

        while (outLength <= kNameComponentMaxBytes
            && name[outLength] != '\0')
        {
            ++outLength;
        }

        return outLength <= kNameComponentMaxBytes
            && IsValidUtf8(name, outLength);
    }
}
