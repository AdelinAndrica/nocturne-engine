#include "SubsystemRegistry.h"
#include "../Log.h"
#include "../Assert.h"

#include <algorithm>
#include <string>

namespace noc {

    void SubsystemRegistry::Register(const SubsystemDesc& desc)
    {
        // Detect duplicates early
        for (const auto& s : subsystems_)
        {
            if (std::string_view{ s.name } == std::string_view{ desc.name })
            {
                NOC_LOG_ERROR("Core", "Duplicate subsystem name: %s", desc.name);
                NOC_ASSERT_MSG(false, "Duplicate subsystem name");
            }
        }

        subsystems_.push_back(desc);
    }

    SubsystemDesc* SubsystemRegistry::Find(std::string_view name)
    {
        for (auto& s : subsystems_)
        {
            if (std::string_view{ s.name } == name)
                return &s;
        }
        return nullptr;
    }

    bool SubsystemRegistry::Visit(SubsystemDesc* s,
        std::unordered_map<std::string_view, VisitState>& state,
        std::vector<std::string_view>& stack)
    {
        const std::string_view name{ s->name };
        auto it = state.find(name);
        if (it != state.end())
        {
            if (it->second == VisitState::Visiting)
            {
                // Cycle detected: print the stack
                NOC_LOG_ERROR("Core", "Subsystem dependency cycle detected:");
                for (auto sv : stack)
                    NOC_LOG_ERROR("Core", "  -> %.*s", (int)sv.size(), sv.data());
                NOC_LOG_ERROR("Core", "  -> %.*s", (int)name.size(), name.data());
                return false;
            }
            if (it->second == VisitState::Visited)
                return true;
        }

        state[name] = VisitState::Visiting;
        stack.push_back(name);

        // Visit dependencies first
        for (const char* depCStr : s->dependencies)
        {
            const std::string_view dep{ depCStr };
            SubsystemDesc* d = Find(dep);
            if (!d)
            {
                NOC_LOG_ERROR("Core", "Missing dependency '%.*s' required by '%.*s'",
                    (int)dep.size(), dep.data(),
                    (int)name.size(), name.data());
                return false;
            }

            if (!Visit(d, state, stack))
                return false;
        }

        // Done exploring
        stack.pop_back();
        state[name] = VisitState::Visited;

        // Add to order after deps (post-order)
        startupOrder_.push_back(s);
        return true;
    }

    bool SubsystemRegistry::BuildStartupOrder()
    {
        startupOrder_.clear();
        startupOrder_.reserve(subsystems_.size());

        std::unordered_map<std::string_view, VisitState> state;
        state.reserve(subsystems_.size());

        std::vector<std::string_view> stack;
        stack.reserve(subsystems_.size());

        // Visit all subsystems
        for (auto& s : subsystems_)
        {
            const std::string_view name{ s.name };
            if (state[name] == VisitState::Visited)
                continue;

            if (!Visit(&s, state, stack))
                return false;
        }

        // startupOrder_ currently has each node appended after its deps
        // This produces a valid topological order already.

        return true;
    }

    bool SubsystemRegistry::StartupAll(void* ctx)
    {
        if (!BuildStartupOrder())
            return false;

        // Start in sorted order
        for (SubsystemDesc* s : startupOrder_)
        {
            if (s->startup && !s->startup(ctx))
            {
                NOC_LOG_ERROR("Core", "Subsystem startup failed: %s", s->name);

                // Shutdown those already started (reverse)
                ShutdownAll(ctx);
                return false;
            }
        }

        return true;
    }

    void SubsystemRegistry::ShutdownAll(void* ctx)
    {
        // shutdown in reverse startup order
        for (auto it = startupOrder_.rbegin(); it != startupOrder_.rend(); ++it)
        {
            if ((*it)->shutdown)
                (*it)->shutdown(ctx);
        }
        startupOrder_.clear();
    }

} // namespace noc
