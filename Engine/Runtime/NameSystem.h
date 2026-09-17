#pragma once

#include "Runtime/ComponentStorage.h"
#include "Runtime/Components/NameComponent.h"

#include <cstdint>

namespace noc
{
    class EntityRegistry;
    class IAllocator;

    // Owns runtime display names for entities.
    //
    // Design choice (not directly from the book):
    // - empty names are valid;
    // - duplicate names are valid;
    // - names longer than kNameComponentMaxBytes are rejected, never truncated.
    class NameSystem
    {
    public:
        NameSystem() = default;
        ~NameSystem();

        NameSystem(const NameSystem&) = delete;
        NameSystem& operator=(const NameSystem&) = delete;
        NameSystem(NameSystem&&) = delete;
        NameSystem& operator=(NameSystem&&) = delete;

        bool Init(
            EntityRegistry& entities,
            IAllocator& allocator,
            uint32_t initialCapacity = 64);
        void Shutdown();

        [[nodiscard]] NameComponent* Add(
            EntityHandle entity,
            const char* initialName = "");

        [[nodiscard]] bool Remove(EntityHandle entity);

        [[nodiscard]] bool Has(EntityHandle entity) const;
        [[nodiscard]] const NameComponent* Get(EntityHandle entity) const;
        [[nodiscard]] uint32_t Count() const;

        [[nodiscard]] bool SetName(EntityHandle entity, const char* name);

        [[nodiscard]] uint32_t DenseCount() const;
        [[nodiscard]] EntityHandle OwnerAtDenseIndex(uint32_t denseIndex) const;
        [[nodiscard]] const NameComponent* ComponentAtDenseIndex(
            uint32_t denseIndex) const;

    private:
        [[nodiscard]] bool IsUsableEntity_(EntityHandle entity) const;
        [[nodiscard]] static bool ValidateName_(
            const char* name,
            uint32_t& outLength);

        EntityRegistry* entities_ = nullptr;
        ComponentStorage<NameComponent> names_;
    };
}
