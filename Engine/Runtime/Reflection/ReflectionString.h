#pragma once

#include "Core/Memory/Allocator.h"
#include "Runtime/Reflection/ReflectionMetadata.h"

#include <cstdint>
#include <cstring>
#include <new>

namespace noc
{
    // Owning UTF-8 byte string used by the reflected runtime schema.
    //
    // Design choice (not directly from the book): reflection owns a compact
    // non-STL string value so public reflection APIs remain allocator-explicit
    // and suitable for undo/snapshot/serialization staging.
    class ReflectionString
    {
    public:
        ReflectionString() = default;

        explicit ReflectionString(IAllocator& allocator) noexcept
            : allocator_(&allocator)
        {
        }

        ~ReflectionString()
        {
            Clear();
        }

        ReflectionString(const ReflectionString&) = delete;
        ReflectionString& operator=(const ReflectionString&) = delete;
        ReflectionString(ReflectionString&&) = delete;
        ReflectionString& operator=(ReflectionString&&) = delete;

        [[nodiscard]] bool Assign(
            IAllocator& allocator,
            const char* text)
        {
            if (!text)
                return false;

            const std::size_t length = std::strlen(text);
            if (length > 0xFFFFFFFFull)
                return false;

            if (length == 0)
            {
                Clear();
                allocator_ = &allocator;
                return true;
            }

            char* replacement = static_cast<char*>(
                allocator.Allocate(length + 1u, alignof(char)));
            if (!replacement)
                return false;

            std::memcpy(replacement, text, length + 1u);

            Clear();
            allocator_ = &allocator;
            data_ = replacement;
            size_ = static_cast<uint32_t>(length);
            capacity_ = size_ + 1u;
            return true;
        }

        [[nodiscard]] bool Assign(const char* text)
        {
            return allocator_ && Assign(*allocator_, text);
        }

        void Clear()
        {
            if (data_ && allocator_)
                allocator_->Deallocate(data_);

            data_ = nullptr;
            size_ = 0;
            capacity_ = 0;
        }

        [[nodiscard]] const char* CStr() const noexcept
        {
            return data_ ? data_ : "";
        }

        [[nodiscard]] uint32_t Size() const noexcept
        {
            return size_;
        }

        [[nodiscard]] uint32_t Capacity() const noexcept
        {
            return capacity_;
        }

        [[nodiscard]] bool Empty() const noexcept
        {
            return size_ == 0;
        }

        [[nodiscard]] bool Equals(
            const ReflectionString& other) const noexcept
        {
            return size_ == other.size_
                && std::memcmp(
                    CStr(),
                    other.CStr(),
                    static_cast<std::size_t>(size_) + 1u) == 0;
        }

    private:
        IAllocator* allocator_ = nullptr;
        char* data_ = nullptr;
        uint32_t size_ = 0;
        uint32_t capacity_ = 0;
    };

    namespace reflection_string_detail
    {
        inline bool DefaultConstruct(
            void* destination,
            IAllocator& allocator)
        {
            new (destination) ReflectionString(allocator);
            return true;
        }

        inline void Destruct(
            void* object,
            IAllocator&)
        {
            static_cast<ReflectionString*>(object)->~ReflectionString();
        }

        inline bool CopyConstruct(
            void* destination,
            const void* source,
            IAllocator& allocator)
        {
            auto* result =
                new (destination) ReflectionString(allocator);

            if (!result->Assign(
                    static_cast<const ReflectionString*>(source)->CStr()))
            {
                result->~ReflectionString();
                return false;
            }

            return true;
        }

        inline bool MoveConstruct(
            void* destination,
            void* source,
            IAllocator& allocator)
        {
            auto* result =
                new (destination) ReflectionString(allocator);
            auto* sourceString =
                static_cast<ReflectionString*>(source);

            if (!result->Assign(sourceString->CStr()))
            {
                result->~ReflectionString();
                return false;
            }

            sourceString->Clear();
            return true;
        }

        inline bool CopyAssign(
            void* destination,
            const void* source,
            IAllocator& allocator)
        {
            return static_cast<ReflectionString*>(destination)->Assign(
                allocator,
                static_cast<const ReflectionString*>(source)->CStr());
        }

        inline bool MoveAssign(
            void* destination,
            void* source,
            IAllocator& allocator)
        {
            auto* sourceString =
                static_cast<ReflectionString*>(source);

            if (!static_cast<ReflectionString*>(destination)->Assign(
                    allocator,
                    sourceString->CStr()))
            {
                return false;
            }

            sourceString->Clear();
            return true;
        }

        inline bool Equals(const void* a, const void* b)
        {
            return static_cast<const ReflectionString*>(a)->Equals(
                *static_cast<const ReflectionString*>(b));
        }

        inline bool Reset(
            void* object,
            IAllocator&)
        {
            static_cast<ReflectionString*>(object)->Clear();
            return true;
        }
    }

    [[nodiscard]] inline TypeLifecycleOperations
    MakeReflectionStringLifecycleOperations() noexcept
    {
        return TypeLifecycleOperations{
            &reflection_string_detail::DefaultConstruct,
            &reflection_string_detail::Destruct,
            &reflection_string_detail::CopyConstruct,
            &reflection_string_detail::MoveConstruct,
            &reflection_string_detail::CopyAssign,
            &reflection_string_detail::MoveAssign,
            &reflection_string_detail::Equals,
            &reflection_string_detail::Reset
        };
    }
}
