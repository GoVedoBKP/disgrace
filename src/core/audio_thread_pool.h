#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace disgrace_ns {

// Simple worker thread pool for parallel track rendering.
// Each call to parallel_for() dispatches fn(i) for i in [0, count) across
// all worker threads, then blocks until all items are done.
//
// Falls back to serial execution when constructed with 0 workers or when
// parallel_for is called with count == 0.
//
// NOTE: Uses condition variables internally, which is not strictly RT-safe
// (avoids priority inversion but may cause occasional scheduler latency).
// Wakeup overhead is typically <50 µs — negligible for JACK buffer sizes ≥128.

class AudioThreadPool {
public:
    explicit AudioThreadPool(size_t num_workers = 0) {
        if (num_workers > 0)
            start_workers(num_workers);
    }

    ~AudioThreadPool() { stop_all(); }

    // Non-copyable.
    AudioThreadPool(const AudioThreadPool&) = delete;
    AudioThreadPool& operator=(const AudioThreadPool&) = delete;

    size_t size() const { return m_workers.size(); }

    // Resize the pool. Safe to call from GUI thread while the audio thread
    // is not inside parallel_for (e.g., when the engine is stopped).
    void resize(size_t num_workers) {
        stop_all();
        if (num_workers > 0)
            start_workers(num_workers);
    }

    // Execute fn(i) for i in [0, count) in parallel across worker threads.
    // Blocks until all items have been processed.
    void parallel_for(size_t count, const std::function<void(size_t)>& fn) {
        if (count == 0) return;
        if (m_workers.empty()) {
            for (size_t i = 0; i < count; ++i) fn(i);
            return;
        }

        {
            std::lock_guard<std::mutex> lk(m_mu);
            m_fn    = &fn;
            m_total = count;
            m_next.store(0, std::memory_order_relaxed);
            m_done.store(0, std::memory_order_relaxed);
            ++m_epoch;
        }
        m_work_cv.notify_all();

        // The calling thread also helps drain the queue.
        const size_t total = count;
        const auto*  f     = &fn;
        for (;;) {
            size_t idx = m_next.fetch_add(1, std::memory_order_relaxed);
            if (idx >= total) break;
            (*f)(idx);
        }

        // Wait for all workers to finish.
        const size_t n_workers = m_workers.size();
        std::unique_lock<std::mutex> lk(m_mu);
        m_done_cv.wait(lk, [&] {
            return m_done.load(std::memory_order_relaxed) >= n_workers;
        });
    }

private:
    void start_workers(size_t n) {
        m_stop = false;
        m_workers.reserve(n);
        for (size_t i = 0; i < n; ++i)
            m_workers.emplace_back([this] { worker_loop(); });
    }

    void stop_all() {
        {
            std::lock_guard<std::mutex> lk(m_mu);
            m_stop = true;
            ++m_epoch;
        }
        m_work_cv.notify_all();
        for (auto& t : m_workers)
            if (t.joinable()) t.join();
        m_workers.clear();
        m_stop = false;
    }

    void worker_loop() {
        int seen_epoch;
        {
            std::lock_guard<std::mutex> lk(m_mu);
            seen_epoch = m_epoch;
        }

        while (true) {
            {
                std::unique_lock<std::mutex> lk(m_mu);
                m_work_cv.wait(lk, [&] {
                    return m_stop || m_epoch != seen_epoch;
                });
                if (m_stop) return;
                seen_epoch = m_epoch;
            }

            // Drain the shared work queue.
            const size_t total = m_total;
            const auto*  fn    = m_fn;
            for (;;) {
                size_t idx = m_next.fetch_add(1, std::memory_order_relaxed);
                if (idx >= total) break;
                (*fn)(idx);
            }

            // Signal completion; last worker wakes the caller.
            if (m_done.fetch_add(1, std::memory_order_acq_rel) + 1 >= m_workers.size())
                m_done_cv.notify_one();
        }
    }

    std::vector<std::thread>            m_workers;
    std::mutex                          m_mu;
    std::condition_variable             m_work_cv;
    std::condition_variable             m_done_cv;
    bool                                m_stop{false};
    int                                 m_epoch{0};
    const std::function<void(size_t)>*  m_fn{nullptr};
    size_t                              m_total{0};
    std::atomic<size_t>                 m_next{0};
    std::atomic<size_t>                 m_done{0};
};

} // namespace disgrace_ns
