#pragma once
#include <cstdint>

namespace noc
{
    /**
     * @brief Non-owning identity for one ResourceManager record.
     *
     * A handle contains an index plus generation so callers do not retain pointers
     * into ResourceManager's internal storage.
     *
     * @par Important beginner rule
     * IsValid() checks only the handle's local sentinel value. It does NOT prove
     * that ResourceManager still accepts the handle or that the resource is ready.
     * Use ResourceManager::IsReady(), HasFailed() and the appropriate Get*() method
     * before reading resource data.
     *
     * @par Ownership
     * ResourceHandle owns no bytes and performs no cleanup. ResourceManager owns the
     * loaded resource and its lifetime.
     *
     * @see ResourceManager
     * @ingroup resources
     */
    struct ResourceHandle
    {
        uint32_t index = 0xFFFFFFFFu;
        uint32_t generation = 0;

        /**
         * @brief Checks whether this value is not the local invalid-index sentinel.
         *
         * @return true when index is not 0xFFFFFFFF; false otherwise.
         *
         * @warning This is not a readiness or liveness query.
         */
        constexpr bool IsValid() const { return index != 0xFFFFFFFFu; }

        /** @brief Compares resource identity (index and generation). */
        friend constexpr bool operator==(ResourceHandle a, ResourceHandle b)
        {
            return a.index == b.index && a.generation == b.generation;
        }

        /** @brief Returns true when resource identity differs. */
        friend constexpr bool operator!=(ResourceHandle a, ResourceHandle b)
        {
            return !(a == b);
        }
    };

} // namespace noc
