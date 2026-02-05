#include "JobSystem.h"

#include "Core/Log.h"

#include <algorithm>

namespace noc {

    bool JobSystem::Init(uint32_t workerCount)
    {
        if (running_.load(std::memory_order_acquire))
            return true;

        // Design choice (not directly from the book): clamp worker count.
        if (workerCount == 0)
            workerCount = std::max(1u, std::thread::hardware_concurrency());

        workerCount_ = workerCount;

        running_.store(true, std::memory_order_release);

        workers_.reserve(workerCount_);
        for (uint32_t i = 0; i < workerCount_; ++i)
            workers_.emplace_back([this, i]() { WorkerMain_(i); });

        NOC_LOG_INFO("Jobs", "JobSystem initialized (%u workers)", workerCount_);
        return true;
    }

    void JobSystem::Shutdown()
    {
        if (!running_.load(std::memory_order_acquire))
            return;

        WaitIdle();

        running_.store(false, std::memory_order_release);
        cvWork_.notify_all();

        for (auto& t : workers_)
        {
            if (t.joinable())
                t.join();
        }
        workers_.clear();

        // Drain anything that might remain (should be empty after WaitIdle).
        {
            std::lock_guard<std::mutex> lock(mtx_);
            while (!queue_.empty())
                queue_.pop();
        }

        NOC_LOG_INFO("Jobs", "JobSystem shutdown complete");
    }

    void JobSystem::Enqueue(JobFn fn)
    {
        if (!fn)
            return;

        if (!running_.load(std::memory_order_acquire))
        {
            // If jobs are not running, execute inline (safe fallback).
            fn();
            return;
        }

        inflight_.fetch_add(1, std::memory_order_acq_rel);

        {
            std::lock_guard<std::mutex> lock(mtx_);
            queue_.push(std::move(fn));
        }

        cvWork_.notify_one();
    }

    void JobSystem::WaitIdle()
    {
        // Wait until there are no inflight jobs and queue is empty.
        std::unique_lock<std::mutex> lock(mtx_);
        cvIdle_.wait(lock, [&]() {
            return queue_.empty() && inflight_.load(std::memory_order_acquire) == 0;
            });
    }

    void JobSystem::WorkerMain_(uint32_t workerIndex)
    {
        (void)workerIndex;

        for (;;)
        {
            JobFn job;

            {
                std::unique_lock<std::mutex> lock(mtx_);
                cvWork_.wait(lock, [&]() {
                    return !running_.load(std::memory_order_acquire) || !queue_.empty();
                    });

                if (!running_.load(std::memory_order_acquire) && queue_.empty())
                    break;

                if (!queue_.empty())
                {
                    job = std::move(queue_.front());
                    queue_.pop();
                }
            }

            if (job)
                job();

            const uint32_t left = inflight_.fetch_sub(1, std::memory_order_acq_rel) - 1;

            // Notify anyone waiting for idle when we *might* have reached it.
            if (left == 0)
            {
                std::lock_guard<std::mutex> lock(mtx_);
                if (queue_.empty())
                    cvIdle_.notify_all();
            }
        }
    }

} // namespace noc
