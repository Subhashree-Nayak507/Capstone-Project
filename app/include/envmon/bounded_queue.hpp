#pragma once
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>

namespace envmon {

/// Thread-safe bounded producer/consumer queue.
/// When full, push() discards the OLDEST element (fresh data matters more
/// for a monitor) and counts it as dropped.
template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity) : cap_(capacity ? capacity : 1) {}

    /// @return false if an old element had to be dropped (or queue closed).
    bool push(T value) {
        bool ok = true;
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (closed_) return false;
            if (q_.size() >= cap_) { q_.pop_front(); ++dropped_; ok = false; }
            q_.push_back(std::move(value));
        }
        cv_.notify_one();
        return ok;
    }

    /// Wait up to @p timeout for an element. @return false on timeout, or when
    /// the queue is closed and drained.
    bool pop(T& out, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lk(mu_);
        cv_.wait_for(lk, timeout, [&] { return !q_.empty() || closed_; });
        if (q_.empty()) return false;
        out = std::move(q_.front());
        q_.pop_front();
        return true;
    }

    void close() {
        { std::lock_guard<std::mutex> lk(mu_); closed_ = true; }
        cv_.notify_all();
    }

    std::size_t size() const { std::lock_guard<std::mutex> lk(mu_); return q_.size(); }
    uint64_t dropped() const { std::lock_guard<std::mutex> lk(mu_); return dropped_; }
    bool closed() const { std::lock_guard<std::mutex> lk(mu_); return closed_; }

private:
    mutable std::mutex      mu_;
    std::condition_variable cv_;
    std::deque<T>           q_;
    std::size_t             cap_;
    uint64_t                dropped_{0};
    bool                    closed_{false};
};

}
