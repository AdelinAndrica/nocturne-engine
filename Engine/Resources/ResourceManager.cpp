#include "ResourceManager.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/Jobs/JobSystem.h"
#include "Core/Memory/Allocator.h"
#include "Resources/ResourceID.h"
#include "Resources/VirtualFileSystem.h"
#include "Resources/VPath.h"
#include "Resources/FileHandle.h"
#include "Runtime/Engine.h"

#include "Resources/Typed/TextResourceLoader.h"

namespace noc
{
    enum class ResourceState : uint8_t
    {
        Empty = 0,
        Requested,
        Loading,
        Ready,
        Failed
    };

    struct Record
    {
        ResourceID id{};
        std::string vpathNormalized;
        std::atomic<ResourceState> state{ ResourceState::Empty };

        uint32_t generation = 1;

        // Binary blob (owned by ResourceManager allocator)
        uint8_t* data = nullptr;
        size_t size = 0;

        // Typed object (type-erased)
        ResourceType type = ResourceType::Unknown;
        void* typedObject = nullptr;

        std::string error;

        Record() = default;
        Record(const Record&) = delete;
        Record& operator=(const Record&) = delete;
        Record(Record&&) = delete;
        Record& operator=(Record&&) = delete;
    };

    struct CompletedReq
    {
        uint32_t index = 0;
        ResourceType type = ResourceType::Unknown;

        bool ok = false;
        std::vector<uint8_t> bytes; // only used for Binary (or intermediate before decode)
        void* typedObject = nullptr;

        std::string error;
    };

    struct InternalState
    {
        std::mutex mtx;

        // Completion queue is produced by worker jobs, consumed by main thread Update().
        std::deque<CompletedReq> completed;

        std::unordered_map<uint64_t, uint32_t> idToIndex;
        std::vector<std::unique_ptr<Record>> records;

        std::atomic<uint32_t> jobsInflight{ 0 };
        std::condition_variable cvJobsIdle;
    };

    static TextResourceLoader g_textLoader;

    static std::string NormalizeVPath_(std::string_view vpath)
    {
        char buf[512]{};
        if (!noc::vpath::NormalizeToRelative(buf, sizeof(buf), vpath))
            return {};
        return std::string(buf);
    }

    static uint64_t MakeCacheKey_(uint64_t idValue, ResourceType type)
    {
        return idValue ^ (static_cast<uint64_t>(type) * 0x9E3779B97F4A7C15ull);
    }

    ResourceManager::~ResourceManager()
    {
        Shutdown();
    }

    bool ResourceManager::Init(Engine& engine, VirtualFileSystem& vfs)
    {
        if (running_)
            return true;

        engine_ = &engine;
        vfs_ = &vfs;

        auto* st = new InternalState();
        st->records.reserve(256);
        state_ = st;

        // Register built-in loaders (Phase 5)
        loaders_.RegisterLoader(&g_textLoader);

        running_ = true;

        NOC_LOG_INFO("Res", "ResourceManager initialized (job-backed)");
        return true;
    }

    void ResourceManager::WaitAllJobs_()
    {
        auto* st = static_cast<InternalState*>(state_);
        if (!st)
            return;

        std::unique_lock<std::mutex> lock(st->mtx);
        st->cvJobsIdle.wait(lock, [&]() {
            return st->jobsInflight.load(std::memory_order_acquire) == 0;
            });
    }

    void ResourceManager::Shutdown()
    {
        if (!running_)
            return;

        running_ = false;

        auto* st = static_cast<InternalState*>(state_);
        if (!st)
            return;

        // Wait for outstanding load jobs to finish producing completions.
        WaitAllJobs_();

        // Drain completions once (safe: no producers remain).
        Update();

        // Free blobs/typed objects
        for (auto& rp : st->records)
        {
            if (!rp) continue;

            if (rp->typedObject)
            {
                if (rp->type == ResourceType::Text)
                    delete static_cast<TextResource*>(rp->typedObject);

                rp->typedObject = nullptr;
            }

            if (rp->data)
            {
                engine_->Allocator().Deallocate(rp->data);
                rp->data = nullptr;
                rp->size = 0;
            }
        }

        delete st;
        state_ = nullptr;

        engine_ = nullptr;
        vfs_ = nullptr;

        NOC_LOG_INFO("Res", "ResourceManager shutdown complete");
    }

    bool ResourceManager::ValidateHandle_(ResourceHandle h, uint32_t* outIndex) const
    {
        if (!h.IsValid() || !state_)
            return false;

        auto* st = static_cast<InternalState*>(state_);
        if (h.index >= st->records.size())
            return false;

        const Record* r = st->records[h.index].get();
        if (!r)
            return false;

        if (r->generation != h.generation)
            return false;

        *outIndex = h.index;
        return true;
    }

    void ResourceManager::EnqueueLoadJob_(uint32_t index)
    {
        auto* st = static_cast<InternalState*>(state_);
        if (!st || !engine_ || !vfs_)
            return;

        st->jobsInflight.fetch_add(1, std::memory_order_acq_rel);

        engine_->Jobs().Enqueue([this, index]() {
            auto* stLocal = static_cast<InternalState*>(state_);
            if (!stLocal || !vfs_)
                return;

            CompletedReq done{};
            done.index = index;

            // Snapshot record pointer safely.
            Record* r = nullptr;
            {
                std::lock_guard<std::mutex> lock(stLocal->mtx);
                if (index < stLocal->records.size())
                    r = stLocal->records[index].get();
            }

            if (!running_ || !r)
            {
                done.ok = false;
                done.error = "Invalid record index or manager stopped";
            }
            else
            {
                done.type = r->type;

                FileHandle fh = vfs_->OpenRead(r->vpathNormalized);
                if (!fh.valid)
                {
                    done.ok = false;
                    done.error = "OpenRead failed";
                }
                else
                {
                    const uint64_t sz64 = vfs_->Size(fh);
                    const size_t sz = (sz64 > SIZE_MAX) ? SIZE_MAX : (size_t)sz64;

                    done.bytes.resize(sz);

                    if (sz > 0)
                    {
                        const size_t read = vfs_->Read(fh, done.bytes.data(), sz);
                        if (read != sz)
                        {
                            done.ok = false;
                            done.error = "Short read";
                            done.bytes.clear();
                        }
                        else
                        {
                            done.ok = true;

                            if (done.type != ResourceType::Binary)
                            {
                                IResourceLoader* loader = loaders_.FindLoader(done.type);
                                if (!loader)
                                {
                                    done.ok = false;
                                    done.error = "No loader registered for requested type";
                                    done.bytes.clear();
                                }
                                else
                                {
                                    const ResourceLoadResult rr = loader->Decode(done.bytes.data(), done.bytes.size());
                                    if (!rr.ok)
                                    {
                                        done.ok = false;
                                        done.error = rr.error;
                                        done.bytes.clear();
                                    }
                                    else
                                    {
                                        done.typedObject = rr.object;
                                        done.bytes.clear(); // decoded object owns data now
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        // empty file is still OK
                        done.ok = true;

                        if (done.type != ResourceType::Binary)
                        {
                            IResourceLoader* loader = loaders_.FindLoader(done.type);
                            if (!loader)
                            {
                                done.ok = false;
                                done.error = "No loader registered for requested type";
                            }
                            else
                            {
                                const ResourceLoadResult rr = loader->Decode(nullptr, 0);
                                if (!rr.ok)
                                {
                                    done.ok = false;
                                    done.error = rr.error;
                                }
                                else
                                {
                                    done.typedObject = rr.object;
                                }
                            }
                        }
                    }

                    vfs_->Close(fh);
                }
            }

            {
                std::lock_guard<std::mutex> lock(stLocal->mtx);
                stLocal->completed.push_back(std::move(done));
            }

            const uint32_t left = stLocal->jobsInflight.fetch_sub(1, std::memory_order_acq_rel) - 1;
            if (left == 0)
            {
                std::lock_guard<std::mutex> lock(stLocal->mtx);
                stLocal->cvJobsIdle.notify_all();
            }
            });
    }

    ResourceHandle ResourceManager::RequestBinary(std::string_view vpath)
    {
        if (!running_ || !state_)
            return {};

        auto* st = static_cast<InternalState*>(state_);

        const std::string norm = NormalizeVPath_(vpath);
        if (norm.empty())
        {
            NOC_LOG_ERROR("Res", "Invalid vpath: %.*s", (int)vpath.size(), vpath.data());
            return {};
        }

        const ResourceID id = MakeResourceID(norm);
        const uint64_t key = MakeCacheKey_(id.value, ResourceType::Binary);

        // Look up existing
        {
            std::lock_guard<std::mutex> lock(st->mtx);
            auto it = st->idToIndex.find(key);
            if (it != st->idToIndex.end())
            {
                const uint32_t idx = it->second;
                Record* r = st->records[idx].get();
                return ResourceHandle{ idx, r->generation };
            }
        }

        // Create new record
        auto rec = std::make_unique<Record>();
        rec->id = id;
        rec->vpathNormalized = norm;
        rec->type = ResourceType::Binary;
        rec->typedObject = nullptr;
        rec->state.store(ResourceState::Requested, std::memory_order_release);

        uint32_t index = 0;
        {
            std::lock_guard<std::mutex> lock(st->mtx);
            index = (uint32_t)st->records.size();
            st->records.push_back(std::move(rec));
            st->idToIndex.emplace(key, index);
        }

        // Enqueue job
        {
            std::lock_guard<std::mutex> lock(st->mtx);
            st->records[index]->state.store(ResourceState::Loading, std::memory_order_release);
        }
        EnqueueLoadJob_(index);

        return ResourceHandle{ index, st->records[index]->generation };
    }

    ResourceHandleT<TextResource> ResourceManager::RequestText(const char* vpath)
    {
        if (!running_ || !state_)
            return {};

        auto* st = static_cast<InternalState*>(state_);

        const std::string norm = NormalizeVPath_(std::string_view{ vpath ? vpath : "" });
        if (norm.empty())
        {
            NOC_LOG_ERROR("Res", "Invalid vpath: %s", vpath ? vpath : "(null)");
            return {};
        }

        const ResourceID id = MakeResourceID(norm);
        const uint64_t key = MakeCacheKey_(id.value, ResourceType::Text);

        // Look up existing
        {
            std::lock_guard<std::mutex> lock(st->mtx);
            auto it = st->idToIndex.find(key);
            if (it != st->idToIndex.end())
            {
                const uint32_t idx = it->second;
                Record* r = st->records[idx].get();
                return ResourceHandleT<TextResource>(ResourceHandle{ idx, r->generation });
            }
        }

        if (!loaders_.FindLoader(ResourceType::Text))
        {
            NOC_LOG_ERROR("Res", "RequestText: no loader registered for ResourceType::Text");
            return {};
        }

        // Create record
        auto rec = std::make_unique<Record>();
        rec->id = id;
        rec->vpathNormalized = norm;
        rec->type = ResourceType::Text;
        rec->typedObject = nullptr;
        rec->state.store(ResourceState::Requested, std::memory_order_release);

        uint32_t index = 0;
        {
            std::lock_guard<std::mutex> lock(st->mtx);
            index = (uint32_t)st->records.size();
            st->records.push_back(std::move(rec));
            st->idToIndex.emplace(key, index);
        }

        // Enqueue job
        {
            std::lock_guard<std::mutex> lock(st->mtx);
            st->records[index]->state.store(ResourceState::Loading, std::memory_order_release);
        }
        EnqueueLoadJob_(index);

        Record* r = st->records[index].get();
        return ResourceHandleT<TextResource>(ResourceHandle{ index, r->generation });
    }

    void ResourceManager::Update()
    {
        if (!state_)
            return;

        auto* st = static_cast<InternalState*>(state_);

        for (;;)
        {
            CompletedReq c{};
            {
                std::lock_guard<std::mutex> lock(st->mtx);
                if (st->completed.empty())
                    break;
                c = std::move(st->completed.front());
                st->completed.pop_front();
            }

            if (c.index >= st->records.size())
                continue;

            Record& r = *st->records[c.index];

            if (!c.ok)
            {
                r.error = std::move(c.error);
                r.state.store(ResourceState::Failed, std::memory_order_release);
                NOC_LOG_ERROR("Res", "FAILED: %s (%s)", r.vpathNormalized.c_str(), r.error.c_str());
                continue;
            }

            // Success path:
            r.error.clear();

            if (r.type == ResourceType::Binary)
            {
                // Replace blob
                if (r.data)
                {
                    engine_->Allocator().Deallocate(r.data);
                    r.data = nullptr;
                    r.size = 0;
                }

                r.size = c.bytes.size();
                if (r.size > 0)
                {
                    r.data = static_cast<uint8_t*>(engine_->Allocator().Allocate(r.size, 16));
                    std::memcpy(r.data, c.bytes.data(), r.size);
                }

                r.state.store(ResourceState::Ready, std::memory_order_release);
                NOC_LOG_INFO("Res", "READY: %s (%zu bytes)", r.vpathNormalized.c_str(), r.size);
            }
            else
            {
                // Replace typed object
                if (r.typedObject)
                {
                    if (r.type == ResourceType::Text)
                        delete static_cast<TextResource*>(r.typedObject);
                    r.typedObject = nullptr;
                }

                r.typedObject = c.typedObject;
                r.state.store(ResourceState::Ready, std::memory_order_release);
                NOC_LOG_INFO("Res", "READY: %s (typed=%u)", r.vpathNormalized.c_str(), (unsigned)r.type);
            }
        }
    }

    bool ResourceManager::IsReady(ResourceHandle h) const
    {
        uint32_t idx = 0;
        if (!ValidateHandle_(h, &idx))
            return false;

        auto* st = static_cast<InternalState*>(state_);
        return st->records[idx]->state.load(std::memory_order_acquire) == ResourceState::Ready;
    }

    bool ResourceManager::HasFailed(ResourceHandle h) const
    {
        uint32_t idx = 0;
        if (!ValidateHandle_(h, &idx))
            return true;

        auto* st = static_cast<InternalState*>(state_);
        return st->records[idx]->state.load(std::memory_order_acquire) == ResourceState::Failed;
    }

    const uint8_t* ResourceManager::GetBytes(ResourceHandle h) const
    {
        uint32_t idx = 0;
        if (!ValidateHandle_(h, &idx))
            return nullptr;

        auto* st = static_cast<InternalState*>(state_);
        const Record& r = *st->records[idx];
        if (r.type != ResourceType::Binary)
            return nullptr;

        if (r.state.load(std::memory_order_acquire) != ResourceState::Ready)
            return nullptr;

        return r.data;
    }

    size_t ResourceManager::GetSize(ResourceHandle h) const
    {
        uint32_t idx = 0;
        if (!ValidateHandle_(h, &idx))
            return 0;

        auto* st = static_cast<InternalState*>(state_);
        const Record& r = *st->records[idx];
        if (r.type != ResourceType::Binary)
            return 0;

        if (r.state.load(std::memory_order_acquire) != ResourceState::Ready)
            return 0;

        return r.size;
    }

    const char* ResourceManager::GetError(ResourceHandle h) const
    {
        uint32_t idx = 0;
        if (!ValidateHandle_(h, &idx))
            return "Invalid handle";

        auto* st = static_cast<InternalState*>(state_);
        const Record& r = *st->records[idx];
        if (r.state.load(std::memory_order_acquire) != ResourceState::Failed)
            return nullptr;

        return r.error.c_str();
    }

    bool ResourceManager::WaitUntilReady(ResourceHandle h, uint32_t timeoutMs)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

        while (std::chrono::steady_clock::now() < deadline)
        {
            Update();

            if (IsReady(h))
                return true;

            if (HasFailed(h))
                return false;

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return IsReady(h);
    }

    const TextResource* ResourceManager::GetText(ResourceHandleT<TextResource> h) const
    {
        if (!state_)
            return nullptr;

        const ResourceHandle uh = h.Untyped();

        uint32_t idx = 0;
        if (!ValidateHandle_(uh, &idx))
            return nullptr;

        auto* st = static_cast<InternalState*>(state_);
        const Record& r = *st->records[idx];

        if (r.type != ResourceType::Text)
            return nullptr;

        if (r.state.load(std::memory_order_acquire) != ResourceState::Ready)
            return nullptr;

        return static_cast<const TextResource*>(r.typedObject);
    }

} // namespace noc
