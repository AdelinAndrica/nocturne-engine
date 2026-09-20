#pragma once
#include "Resources/ResourceHandle.h"

namespace noc {

    /**
     * @brief Strongly-typed wrapper around ResourceHandle.
     *
     * ResourceHandleT prevents accidentally passing a texture handle to an API expecting
     * a mesh handle at compile time. It does not own the resource and does not change
     * ResourceManager lifetime/readiness semantics.
     *
     * @tparam T Resource type associated with this handle.
     *
     * @par Beginner rule
     * IsValid() is still only the local handle-sentinel check. Use ResourceManager
     * readiness/getter APIs before dereferencing resource data.
     *
     * @ingroup resources
     */
    template <class T>
    class ResourceHandleT {
    public:
        ResourceHandleT() = default;

        /** @brief Wraps an existing untyped resource handle. */
        explicit ResourceHandleT(ResourceHandle h) : h_(h) {}

        /** @brief Returns the underlying non-owning ResourceHandle value. */
        ResourceHandle Untyped() const { return h_; }

        /**
         * @brief Performs the underlying handle's local sentinel check.
         * @warning Does not imply the resource is loaded/ready.
         */
        bool IsValid() const { return h_.IsValid(); }

        friend bool operator==(const ResourceHandleT& a, const ResourceHandleT& b) { return a.h_ == b.h_; }
        friend bool operator!=(const ResourceHandleT& a, const ResourceHandleT& b) { return !(a == b); }

    private:
        ResourceHandle h_{};
    };

} // namespace noc
