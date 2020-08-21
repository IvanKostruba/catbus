#pragma once

#include "exception.h"

#include <atomic>
#include <functional>
#include <optional>

namespace catbus {

  // This queue is implemented as a ring buffer. Size should be a power of 2, so that bitwise
  // AND can be used for masking.
  template <size_t N = 4096>
  class SimpleLockFreeQueue
  {
  public:

    void Enqueue(std::function<void()> task)
    {
      unsigned prod = produced_.fetch_add(1, std::memory_order_relaxed) & mask_;
      while (buffer_[prod].ready.load(std::memory_order_acquire))
      {
        //throw queue_overflow{};
        std::this_thread::yield();
      }
      buffer_[prod].run = std::move(task);
      buffer_[prod].ready.store(true, std::memory_order_release);
    }

    std::optional<std::function<void()>> TryDequeue()
    {
      if (consumed_ >= produced_)
      {
        return std::nullopt;
      }
      unsigned current = consumed_.fetch_add(1, std::memory_order_relaxed) & mask_;
      while (!buffer_[current].ready.load(std::memory_order_acquire))
      {
        std::this_thread::yield(); // TODO: fix infinite waiting.
      }
      auto result = std::optional<std::function<void()>>{ std::move(buffer_[current].run) };
      buffer_[current].ready.store(false, std::memory_order_release);
      return result;
    }

    size_t Size() const
    {
      auto c = consumed_.load(std::memory_order_relaxed);
      auto p = produced_.load(std::memory_order_relaxed);
      return p - c;
    }

  private:
    struct Task
    {
      std::atomic_bool ready{ false };
      std::function<void()> run;
    };
    Task buffer_[N];
    static const size_t mask_{ N - 1 };

    std::atomic_uint consumed_{ 0 };
    std::atomic_uint produced_{ 0 };
  };

}; // namespace catbus
