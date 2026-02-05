#pragma once
#include "Resources/ResourceHandle.h"

namespace noc {

    // Strongly-typed wrapper around an untyped ResourceHandle.
    template <class T>
    class ResourceHandleT {
    public:
        ResourceHandleT() = default;
        explicit ResourceHandleT(ResourceHandle h) : h_(h) {}

        ResourceHandle Untyped() const { return h_; }
        bool IsValid() const { return h_.IsValid(); }

        friend bool operator==(const ResourceHandleT& a, const ResourceHandleT& b) { return a.h_ == b.h_; }
        friend bool operator!=(const ResourceHandleT& a, const ResourceHandleT& b) { return !(a == b); }

    private:
        ResourceHandle h_{};
    };

} // namespace noc