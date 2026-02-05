#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace noc {

    class JobSystem {
    public:
        using JobFn = std::function<void()>;

        JobSystem() = default;
        ~JobSystem() { Shutdown(); }

        JobSystem(const JobSystem&) = delete;
        JobSystem& operator=(const JobSystem&) = delete;

        bool Init(uint32_t workerCount);
        void Shutdown();

        // Fire-and-forget jobs. (Phase 6 scope)
        void Enqueue(JobFn fn);

        // For orderly shutdown / subsystems that need to wait.
        void WaitIdle();

        uint32_t WorkerCount() const { return workerCount_; }
        bool IsRunning() const { return running_.load(std::memory_order_acquire); }

    private:
        void WorkerMain_(uint32_t workerIndex);

    private:
        std::atomic<bool> running_{ false };

        std::mutex mtx_;
        std::condition_variable cvWork_;
        std::condition_variable cvIdle_;
        std::queue<JobFn> queue_;

        std::atomic<uint32_t> inflight_{ 0 };

        std::vector<std::thread> workers_;
        uint32_t workerCount_ = 0;
    };

} // namespace noc
