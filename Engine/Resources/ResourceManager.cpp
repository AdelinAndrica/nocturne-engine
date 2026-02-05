#include "ResourceManager.h"

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cstring>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <memory>
#include <vector>

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Resources/ResourceID.h"
#include "Resources/VirtualFileSystem.h"
#include "Resources/VPath.h"
#include "Resources/FileHandle.h"
#include "Runtime/Engine.h"
#include "Resources/Typed/TextResource.h"
#include "Resources/Typed/TextResourceLoader.h"

namespace noc
{
	static TextResourceLoader g_textLoader;

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

		uint8_t* data = nullptr;
		size_t size = 0;

		std::string error;

		// -------- Phase 5 additions --------
		ResourceType type = ResourceType::Binary; // default matches Phase 4 behavior
		void* typedObject = nullptr;              // points to decoded runtime object when READY

		Record() = default;
		Record(const Record&) = delete;
		Record& operator=(const Record&) = delete;
		Record(Record&&) = delete;
		Record& operator=(Record&&) = delete;
	};


	struct PendingReq { uint32_t index = 0; };

	struct CompletedReq
	{
		uint32_t index = 0;
		bool ok = false;

		// Phase 5: what kind of payload is being completed?
		ResourceType type = ResourceType::Binary;

		// Binary payload (Phase 4)
		std::vector<uint8_t> bytes;

		// Typed payload (Phase 5): heap object produced by loader Decode()
		void* typedObject = nullptr;

		std::string error;
	};


	struct InternalState
	{
		std::mutex mtx;
		std::condition_variable cv;
		std::queue<PendingReq> pending;
		std::queue<CompletedReq> completed;

		std::unordered_map<uint64_t, uint32_t> idToIndex;

		std::vector<std::unique_ptr<Record>> records;
	};

	static std::string NormalizeVPath_(std::string_view vpath)
	{
		char buf[512]{};
		if (!noc::vpath::NormalizeToRelative(buf, sizeof(buf), vpath))
			return {};
		return std::string(buf);
	}

	static uint64_t MakeCacheKey_(uint64_t idValue, ResourceType type)
	{
		// Combine id + type into a single stable key.
		// idValue is already a hash; xor-mix the type.
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

		// Register built-in loaders
		loaders_.RegisterLoader(&g_textLoader);

		running_ = true;
		loaderThread_ = new std::thread([this]() { LoaderThreadMain_(); });

		NOC_LOG_INFO("Res", "ResourceManager initialized (loader thread started)");
		return true;
	}

	void ResourceManager::Shutdown()
	{
		if (!running_)
			return;

		running_ = false;

		auto* st = static_cast<InternalState*>(state_);
		st->cv.notify_all();

		if (loaderThread_)
		{
			auto* t = static_cast<std::thread*>(loaderThread_);
			if (t->joinable())
				t->join();
			delete t;
			loaderThread_ = nullptr;
		}

		// Free blobs
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

		// Get key
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

		// Create new record (stable index)
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

		// Enqueue load
		Record* r = st->records[index].get();
		r->state.store(ResourceState::Loading, std::memory_order_release);

		{
			std::lock_guard<std::mutex> lock(st->mtx);
			st->pending.push(PendingReq{ index });
		}
		st->cv.notify_one();

		return ResourceHandle{ index, r->generation };
	}

	void ResourceManager::Update()
	{
		if (!running_ || !state_)
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
				st->completed.pop();
			}

			if (c.index >= st->records.size())
				continue;

			// Lock while mutating record's non-atomic fields (string, pointers, sizes).
			std::lock_guard<std::mutex> lock(st->mtx);

			Record& r = *st->records[c.index];

			if (!c.ok)
			{
				if (c.typedObject)
				{
					// Phase 5 currently only has TextResource typed objects.
					delete static_cast<TextResource*>(c.typedObject);
					c.typedObject = nullptr;
				}

				r.error = std::move(c.error);
				r.state.store(ResourceState::Failed, std::memory_order_release);
				NOC_LOG_ERROR("Res", "FAILED: %s (%s)", r.vpathNormalized.c_str(), r.error.c_str());
				continue;
			}

			// success
			r.error.clear();

			if (c.type == ResourceType::Binary)
			{
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
			}
			else if (c.type == ResourceType::Text)
			{
				if (r.typedObject)
				{
					delete static_cast<TextResource*>(r.typedObject);
					r.typedObject = nullptr;
				}

				r.typedObject = c.typedObject;
				c.typedObject = nullptr;
			}

			r.state.store(ResourceState::Ready, std::memory_order_release);
			NOC_LOG_INFO("Res", "READY: %s", r.vpathNormalized.c_str());

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
		if (r.state.load(std::memory_order_acquire) != ResourceState::Ready)
			return 0;

		return r.size;
	}

	const char* ResourceManager::GetError(ResourceHandle h) const
	{
		uint32_t idx = 0;
		if (!ValidateHandle_(h, &idx))
			return nullptr;

		auto* st = static_cast<InternalState*>(state_);

		// The error string is a std::string and is mutated during Update() on failure,
		// so we must guard access with the same mutex that protects record access.
		std::lock_guard<std::mutex> lock(st->mtx);

		const Record& r = *st->records[idx];
		if (r.state.load(std::memory_order_acquire) != ResourceState::Failed)
			return nullptr;

		if (r.error.empty())
			return nullptr;

		return r.error.c_str();
	}



	bool ResourceManager::WaitUntilReady(ResourceHandle h, uint32_t timeoutMs)
	{
		const auto start = std::chrono::steady_clock::now();
		while (true)
		{
			Update();

			if (IsReady(h))
				return true;
			if (HasFailed(h))
				return false;

			if (timeoutMs != 0)
			{
				const auto now = std::chrono::steady_clock::now();
				const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
				if ((uint32_t)elapsed >= timeoutMs)
					return false;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
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

		// Ensure loader exists (Phase 5 contract)
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

		// Enqueue load
		Record* r = st->records[index].get();
		r->state.store(ResourceState::Loading, std::memory_order_release);

		{
			std::lock_guard<std::mutex> lock(st->mtx);
			st->pending.push(PendingReq{ index });
		}
		st->cv.notify_one();

		return ResourceHandleT<TextResource>(ResourceHandle{ index, r->generation });
	}

	const TextResource* ResourceManager::GetText(ResourceHandleT<TextResource> h) const
	{
		if (!running_ || !state_)
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


	void ResourceManager::LoaderThreadMain_()
	{
		NOC_LOG_INFO("Res", "Loader thread running");

		auto* st = static_cast<InternalState*>(state_);

		while (running_)
		{
			PendingReq req{};
			{
				std::unique_lock<std::mutex> lock(st->mtx);
				st->cv.wait(lock, [&]() { return !running_ || !st->pending.empty(); });

				if (!running_)
					break;

				req = st->pending.front();
				st->pending.pop();
			}

			CompletedReq done{};
			done.index = req.index;

			// Load via Phase 3 VFS API (OpenRead returns FileHandle)
			Record* r = nullptr;
			{
				std::lock_guard<std::mutex> lock(st->mtx);
				if (req.index < st->records.size())
					r = st->records[req.index].get();
			}

			if (!r)
			{
				done.ok = false;
				done.error = "Invalid record index";
			}
			else
			{
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
							done.type = r->type;

							if (done.ok && r->type != ResourceType::Binary)
							{
								IResourceLoader* loader = loaders_.FindLoader(r->type);
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
										done.bytes.clear(); // no longer needed after decoding
									}
								}
							}

						}
					}
					else
					{
						done.ok = true;
					}

					vfs_->Close(fh);
				}
			}

			{
				std::lock_guard<std::mutex> lock(st->mtx);
				st->completed.push(std::move(done));
			}
		}

		NOC_LOG_INFO("Res", "Loader thread exiting");
	}

} // namespace noc
