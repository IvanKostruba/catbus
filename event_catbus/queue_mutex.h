#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <queue>

namespace catbus {

  class MutexProtectedQueue
  {
  public:

    void Enqueue(std::function<void()> task)
    {
      auto lock = std::unique_lock<std::mutex>{ queue_access_ };
      queue_.push(std::move(task));
    }

    std::optional<std::function<void()>> TryDequeue()
    {
      auto lock = std::unique_lock<std::mutex>{ queue_access_, std::defer_lock };
      if (lock.try_lock())
      {
        if (queue_.empty())
        {
          return std::nullopt;
        }
        auto result = std::optional<std::function<void()>>{ std::move(queue_.front()) };
        queue_.pop();
        return result;
      }
      return std::nullopt;
    }

  private:
    std::queue<std::function<void()>> queue_;
    std::mutex queue_access_;
  };

}; // namespace catbus
