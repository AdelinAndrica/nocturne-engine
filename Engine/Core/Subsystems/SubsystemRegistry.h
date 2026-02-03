#pragma once
#include "Subsystem.h"

#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace noc {

    class SubsystemRegistry
    {
    public:
        void Register(const SubsystemDesc& desc);

        bool StartupAll(void* ctx);
        void ShutdownAll(void* ctx);

    private:
        enum class VisitState : uint8_t { Unvisited, Visiting, Visited };

        bool BuildStartupOrder(); // topo sort + validation

        bool Visit(SubsystemDesc* s,
            std::unordered_map<std::string_view, VisitState>& state,
            std::vector<std::string_view>& stack);

        SubsystemDesc* Find(std::string_view name);

    private:
        std::vector<SubsystemDesc> subsystems_;
        std::vector<SubsystemDesc*> startupOrder_;
    };

} // namespace noc
