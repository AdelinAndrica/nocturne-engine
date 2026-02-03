#pragma once
#include <cstdint>
#include <string_view>

namespace noc
{
    // Design choice (not directly from the book):
    // A 64-bit hash of the normalized virtual path.
    struct ResourceID
    {
        uint64_t value = 0;

        constexpr bool IsValid() const { return value != 0; }

        friend constexpr bool operator==(ResourceID a, ResourceID b) { return a.value == b.value; }
        friend constexpr bool operator!=(ResourceID a, ResourceID b) { return a.value != b.value; }
    };

    // Design choice (not directly from the book): 64-bit FNV-1a.
    inline constexpr uint64_t Fnv1a64(const char* data, size_t len)
    {
        uint64_t h = 1469598103934665603ull;
        for (size_t i = 0; i < len; ++i)
        {
            h ^= static_cast<uint8_t>(data[i]);
            h *= 1099511628211ull;
        }
        return h;
    }

    inline ResourceID MakeResourceID(std::string_view normalizedVPath)
    {
        ResourceID id{};
        id.value = Fnv1a64(normalizedVPath.data(), normalizedVPath.size());

        // Avoid 0 being a valid ID (tiny guard; extremely unlikely anyway).
        if (id.value == 0) id.value = 1;
        return id;
    }

} // namespace noc
