#pragma once

#include "task_wrapper.h"

#include <atomic>

namespace catbus {

// This queue is implemented as a ring buffer. size should be a power of 2, so that bitwise
// AND can be used for masking.
// This kind of queue shown 2x - 2.5x better performance than mutex queue in case of very quick
// event handlers. When there are handlers that can block for significant amount of time,
// performance difference is small to nonobservable.
//
// Also, in case of extremely quick event handling, queue is very succeptible to any thread being
// evicted. My theory is that, since processing is quick, some other thread inevitably arriving
// to the same slot (after masked counter wraps around) and two consumer threads start yielding
// while waiting on the same atomic flag. What happens next is up to chance but test runs show
// that one of the threads may end up reading invalid value and crashing. Setting queue to a
// larger size i.e. 65536 helped against it.
template <size_t N = 4096>
class SimpleLockFreeQueue {
public:

    ~SimpleLockFreeQueue()
    {
        // A hack to avoid a worker thread getting stuck after it was stopped in case it's waiting
        // for the ready flag in yield cycle. This logic kind of belongs to EventBus, but on the
        // other hand it's only relevant to this type of queue.
        while(size() <= 1) {
            enqueue(TaskWrapper{});
        }
    }

    void enqueue(TaskWrapper task) {
        unsigned prod = produced_.fetch_add(1, std::memory_order_relaxed) & mask_;
        while (buffer_[prod].ready.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        buffer_[prod].t = std::move(task);
        buffer_[prod].ready.store(true, std::memory_order_release);
    }

    TaskWrapper try_dequeue() {
        if (consumed_.load(std::memory_order_relaxed) >= produced_.load(std::memory_order_relaxed)) {
            return TaskWrapper{};
        }
        unsigned current = consumed_.fetch_add(1, std::memory_order_relaxed) & mask_;
        while (!buffer_[current].ready.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        auto result = std::move(buffer_[current].t);
        buffer_[current].ready.store(false, std::memory_order_release);
        return result;
    }

    size_t size() const {
        auto c = consumed_.load(std::memory_order_relaxed);
        auto p = produced_.load(std::memory_order_relaxed);
        if (p - c <= N) {
            return p - c;
        }
        return 0;
    }

private:
    struct Task {
        std::atomic_bool ready{ false };
        TaskWrapper t;
    };
    Task buffer_[N];
    static const size_t mask_{ N - 1 };

    std::atomic_uint consumed_{ 0 };
    std::atomic_uint produced_{ 0 };
};

}; // namespace catbus
