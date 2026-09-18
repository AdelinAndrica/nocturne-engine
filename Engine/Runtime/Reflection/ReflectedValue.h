#pragma once

#include "Core/Memory/Allocator.h"
#include "Runtime/Reflection/ReflectionMetadata.h"

#include <cstdint>
#include <utility>

namespace noc
{
    // Allocator-owned generic reflected value.
    //
    // Design choice (not directly from the book): the value snapshots the
    // lifecycle operations/size/alignment instead of retaining a TypeMetadata
    // pointer. This makes destruction safe even if the originating registry is
    // shut down after the value was created.
    class OwnedReflectedValue
    {
    public:
        OwnedReflectedValue() = default;

        ~OwnedReflectedValue()
        {
            Clear();
        }

        OwnedReflectedValue(const OwnedReflectedValue&) = delete;
        OwnedReflectedValue& operator=(const OwnedReflectedValue&) = delete;

        OwnedReflectedValue(OwnedReflectedValue&& other) noexcept
        {
            MoveFrom_(other);
        }

        OwnedReflectedValue& operator=(OwnedReflectedValue&& other) noexcept
        {
            if (this != &other)
            {
                Clear();
                MoveFrom_(other);
            }
            return *this;
        }

        [[nodiscard]] bool InitDefault(
            IAllocator& allocator,
            const TypeMetadata& metadata)
        {
            if (data_ || !metadata.typeId.IsValid()
                || metadata.size == 0
                || metadata.alignment == 0
                || !metadata.lifecycle.defaultConstruct
                || !metadata.lifecycle.destruct)
            {
                return false;
            }

            void* memory =
                allocator.Allocate(metadata.size, metadata.alignment);
            if (!memory)
                return false;

            if (!metadata.lifecycle.defaultConstruct(memory, allocator))
            {
                allocator.Deallocate(memory);
                return false;
            }

            Capture_(allocator, metadata, memory);
            return true;
        }

        [[nodiscard]] bool InitCopy(
            IAllocator& allocator,
            const TypeMetadata& metadata,
            const void* source)
        {
            if (data_ || !source
                || !metadata.typeId.IsValid()
                || metadata.size == 0
                || metadata.alignment == 0
                || !metadata.lifecycle.copyConstruct
                || !metadata.lifecycle.destruct)
            {
                return false;
            }

            void* memory =
                allocator.Allocate(metadata.size, metadata.alignment);
            if (!memory)
                return false;

            if (!metadata.lifecycle.copyConstruct(
                    memory,
                    source,
                    allocator))
            {
                allocator.Deallocate(memory);
                return false;
            }

            Capture_(allocator, metadata, memory);
            return true;
        }

        [[nodiscard]] bool CopyAssign(const void* source)
        {
            if (!data_ || !source || !lifecycle_.copyAssign)
                return false;

            return lifecycle_.copyAssign(
                data_,
                source,
                *allocator_);
        }

        [[nodiscard]] bool CopyAssign(ReflectedConstValueView source)
        {
            return source.typeId == typeId_
                && CopyAssign(source.data);
        }

        [[nodiscard]] bool ResetToDefault()
        {
            if (!data_ || !lifecycle_.reset)
                return false;

            return lifecycle_.reset(
                data_,
                *allocator_);
        }

        [[nodiscard]] bool Equals(ReflectedConstValueView other) const
        {
            if (!data_ || !other.data
                || other.typeId != typeId_
                || !lifecycle_.equals)
            {
                return false;
            }

            return lifecycle_.equals(data_, other.data);
        }

        void Clear()
        {
            if (!data_)
                return;

            lifecycle_.destruct(data_, *allocator_);
            allocator_->Deallocate(data_);

            allocator_ = nullptr;
            data_ = nullptr;
            typeId_ = TypeId::Invalid();
            size_ = 0;
            alignment_ = 0;
            lifecycle_ = {};
        }

        [[nodiscard]] bool IsValid() const noexcept
        {
            return data_ != nullptr && typeId_.IsValid();
        }

        [[nodiscard]] TypeId Type() const noexcept
        {
            return typeId_;
        }

        [[nodiscard]] uint32_t Size() const noexcept
        {
            return size_;
        }

        [[nodiscard]] uint32_t Alignment() const noexcept
        {
            return alignment_;
        }

        [[nodiscard]] void* Data() noexcept
        {
            return data_;
        }

        [[nodiscard]] const void* Data() const noexcept
        {
            return data_;
        }

        [[nodiscard]] ReflectedValueView View() noexcept
        {
            return ReflectedValueView{ typeId_, data_ };
        }

        [[nodiscard]] ReflectedConstValueView ConstView() const noexcept
        {
            return ReflectedConstValueView{ typeId_, data_ };
        }

    private:
        void Capture_(
            IAllocator& allocator,
            const TypeMetadata& metadata,
            void* memory) noexcept
        {
            allocator_ = &allocator;
            data_ = memory;
            typeId_ = metadata.typeId;
            size_ = metadata.size;
            alignment_ = metadata.alignment;
            lifecycle_ = metadata.lifecycle;
        }

        void MoveFrom_(OwnedReflectedValue& other) noexcept
        {
            allocator_ = std::exchange(other.allocator_, nullptr);
            data_ = std::exchange(other.data_, nullptr);
            typeId_ = std::exchange(other.typeId_, TypeId::Invalid());
            size_ = std::exchange(other.size_, 0u);
            alignment_ = std::exchange(other.alignment_, 0u);
            lifecycle_ = other.lifecycle_;
            other.lifecycle_ = {};
        }

        IAllocator* allocator_ = nullptr;
        void* data_ = nullptr;
        TypeId typeId_{};
        uint32_t size_ = 0;
        uint32_t alignment_ = 0;
        TypeLifecycleOperations lifecycle_{};
    };
}
