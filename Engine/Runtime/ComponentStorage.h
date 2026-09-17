#pragma once

#include "Core/Memory/Allocator.h"
#include "Runtime/Entity.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace noc
{
    inline constexpr uint32_t kInvalidComponentIndex = 0xFFFFFFFFu;

    // Dense component storage with sparse EntityHandle -> dense-index lookup.
    //
    // Book grounding:
    // - Gregory, Game Engine Architecture 3rd ed., section 16.2 describes
    //   component/property-centric runtime object models and contiguous
    //   same-type data as a cache-friendly organization.
    // - Nystrom, Game Programming Patterns, Data Locality, motivates processing
    //   homogeneous component data contiguously.
    //
    // Design choice (not directly from the book): Nocturne Phase 15 uses one
    // dense pool per component type and a sparse lookup indexed by entity slot.
    //
    // Mutation invalidates pointers/references returned by this storage.
    // World is responsible for validating EntityRegistry::IsAlive() before
    // structural mutation. Generation-aware owner checks prevent stale handles
    // from resolving to components owned by a newer entity generation.
    template <typename T>
    class ComponentStorage
    {
        static_assert(std::is_nothrow_move_constructible_v<T>,
            "ComponentStorage<T> requires a noexcept move constructor so growth and swap-remove cannot partially corrupt the pool.");
        static_assert(std::is_destructible_v<T>,
            "ComponentStorage<T> requires a destructible component type.");

    public:
        ComponentStorage() = default;

        ~ComponentStorage()
        {
            Shutdown();
        }

        ComponentStorage(const ComponentStorage&) = delete;
        ComponentStorage& operator=(const ComponentStorage&) = delete;
        ComponentStorage(ComponentStorage&&) = delete;
        ComponentStorage& operator=(ComponentStorage&&) = delete;

        bool Init(
            IAllocator& allocator,
            uint32_t initialDenseCapacity = 64,
            uint32_t initialSparseCapacity = 64)
        {
            if (allocator_)
                return true;

            allocator_ = &allocator;

            if (initialDenseCapacity > 0 && !EnsureDenseCapacity_(initialDenseCapacity))
            {
                Shutdown();
                return false;
            }

            if (initialSparseCapacity > 0 && !EnsureSparseCapacity_(initialSparseCapacity))
            {
                Shutdown();
                return false;
            }

            return true;
        }

        void Shutdown()
        {
            if (!allocator_)
                return;

            for (uint32_t i = 0; i < count_; ++i)
                components_[i].~T();

            if (components_) allocator_->Deallocate(components_);
            if (owners_) allocator_->Deallocate(owners_);
            if (sparse_) allocator_->Deallocate(sparse_);

            allocator_ = nullptr;
            components_ = nullptr;
            owners_ = nullptr;
            sparse_ = nullptr;
            count_ = 0;
            denseCapacity_ = 0;
            sparseCapacity_ = 0;
        }

        template <typename... Args>
        T* Emplace(EntityHandle entity, Args&&... args)
        {
            if (!allocator_ || !entity.IsValid())
                return nullptr;

            if (!EnsureSparseCapacity_(entity.index + 1u))
                return nullptr;

            // Any occupied sparse slot blocks insertion, even when the stored
            // generation differs. That indicates the caller failed to remove
            // the old entity generation's component before slot reuse.
            if (sparse_[entity.index] != kInvalidComponentIndex)
                return nullptr;

            if (!EnsureDenseCapacity_(count_ + 1u))
                return nullptr;

            T* slot = components_ + count_;
            new (slot) T(std::forward<Args>(args)...);

            owners_[count_] = entity;
            sparse_[entity.index] = count_;
            ++count_;
            return slot;
        }

        T* Add(EntityHandle entity)
        {
            static_assert(std::is_default_constructible_v<T>,
                "ComponentStorage<T>::Add requires a default-constructible component. Use Emplace for other component types.");
            return Emplace(entity);
        }

        bool Remove(EntityHandle entity)
        {
            const uint32_t denseIndex = DenseIndex(entity);
            if (denseIndex == kInvalidComponentIndex)
                return false;

            const uint32_t lastIndex = count_ - 1u;
            sparse_[entity.index] = kInvalidComponentIndex;

            components_[denseIndex].~T();

            if (denseIndex != lastIndex)
            {
                new (components_ + denseIndex) T(std::move(components_[lastIndex]));
                components_[lastIndex].~T();

                owners_[denseIndex] = owners_[lastIndex];
                sparse_[owners_[denseIndex].index] = denseIndex;
            }

            --count_;
            return true;
        }

        [[nodiscard]] bool Has(EntityHandle entity) const
        {
            return DenseIndex(entity) != kInvalidComponentIndex;
        }

        [[nodiscard]] T* Get(EntityHandle entity)
        {
            const uint32_t denseIndex = DenseIndex(entity);
            return denseIndex == kInvalidComponentIndex ? nullptr : components_ + denseIndex;
        }

        [[nodiscard]] const T* Get(EntityHandle entity) const
        {
            const uint32_t denseIndex = DenseIndex(entity);
            return denseIndex == kInvalidComponentIndex ? nullptr : components_ + denseIndex;
        }

        [[nodiscard]] T* TryGet(EntityHandle entity)
        {
            return Get(entity);
        }

        [[nodiscard]] const T* TryGet(EntityHandle entity) const
        {
            return Get(entity);
        }

        [[nodiscard]] uint32_t DenseIndex(EntityHandle entity) const
        {
            if (!entity.IsValid() || entity.index >= sparseCapacity_)
                return kInvalidComponentIndex;

            const uint32_t denseIndex = sparse_[entity.index];
            if (denseIndex == kInvalidComponentIndex || denseIndex >= count_)
                return kInvalidComponentIndex;

            return owners_[denseIndex] == entity ? denseIndex : kInvalidComponentIndex;
        }

        [[nodiscard]] uint32_t Count() const
        {
            return count_;
        }

        [[nodiscard]] uint32_t DenseCapacity() const
        {
            return denseCapacity_;
        }

        [[nodiscard]] uint32_t SparseCapacity() const
        {
            return sparseCapacity_;
        }

        [[nodiscard]] T* ComponentAtDenseIndex(uint32_t denseIndex)
        {
            return denseIndex < count_ ? components_ + denseIndex : nullptr;
        }

        [[nodiscard]] const T* ComponentAtDenseIndex(uint32_t denseIndex) const
        {
            return denseIndex < count_ ? components_ + denseIndex : nullptr;
        }

        [[nodiscard]] EntityHandle OwnerAtDenseIndex(uint32_t denseIndex) const
        {
            return denseIndex < count_ ? owners_[denseIndex] : EntityHandle::Invalid();
        }

    private:
        static constexpr std::size_t StorageAlignment_()
        {
            return alignof(T) > alignof(std::max_align_t)
                ? alignof(T)
                : alignof(std::max_align_t);
        }

        static uint32_t GrowCapacity_(uint32_t current, uint32_t required)
        {
            if (required == 0 || required >= kInvalidComponentIndex)
                return 0;

            uint32_t target = current == 0 ? 1u : current;
            while (target < required)
            {
                if (target > (kInvalidComponentIndex / 2u))
                    return required;
                target *= 2u;
            }
            return target;
        }

        bool EnsureDenseCapacity_(uint32_t required)
        {
            if (required <= denseCapacity_)
                return true;

            const uint32_t target = GrowCapacity_(denseCapacity_, required);
            if (target == 0)
                return false;

            auto* newComponents = static_cast<T*>(
                allocator_->Allocate(sizeof(T) * target, StorageAlignment_()));
            auto* newOwners = static_cast<EntityHandle*>(
                allocator_->Allocate(
                    sizeof(EntityHandle) * target,
                    alignof(std::max_align_t)));

            if (!newComponents || !newOwners)
            {
                if (newComponents) allocator_->Deallocate(newComponents);
                if (newOwners) allocator_->Deallocate(newOwners);
                return false;
            }

            for (uint32_t i = 0; i < count_; ++i)
                new (newComponents + i) T(std::move(components_[i]));

            if (count_ > 0)
                std::memcpy(newOwners, owners_, sizeof(EntityHandle) * count_);

            for (uint32_t i = 0; i < count_; ++i)
                components_[i].~T();

            if (components_) allocator_->Deallocate(components_);
            if (owners_) allocator_->Deallocate(owners_);

            components_ = newComponents;
            owners_ = newOwners;
            denseCapacity_ = target;
            return true;
        }

        bool EnsureSparseCapacity_(uint32_t required)
        {
            if (required <= sparseCapacity_)
                return true;

            const uint32_t target = GrowCapacity_(sparseCapacity_, required);
            if (target == 0)
                return false;

            auto* newSparse = static_cast<uint32_t*>(
                allocator_->Allocate(
                    sizeof(uint32_t) * target,
                    alignof(std::max_align_t)));
            if (!newSparse)
                return false;

            if (sparseCapacity_ > 0)
                std::memcpy(newSparse, sparse_, sizeof(uint32_t) * sparseCapacity_);

            for (uint32_t i = sparseCapacity_; i < target; ++i)
                newSparse[i] = kInvalidComponentIndex;

            if (sparse_) allocator_->Deallocate(sparse_);
            sparse_ = newSparse;
            sparseCapacity_ = target;
            return true;
        }

        IAllocator* allocator_ = nullptr;

        T* components_ = nullptr;
        EntityHandle* owners_ = nullptr;
        uint32_t* sparse_ = nullptr;

        uint32_t count_ = 0;
        uint32_t denseCapacity_ = 0;
        uint32_t sparseCapacity_ = 0;
    };
}
