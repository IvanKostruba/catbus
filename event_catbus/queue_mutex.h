#pragma once

#include "task_wrapper.h"

#include <mutex>
#include <queue>

namespace catbus {

  class MutexProtectedQueue
  {
  public:

    void Enqueue(TaskWrapper task)
    {
      auto lock = std::unique_lock<std::mutex>{ queue_access_ };
      queue_.push(std::move(task));
    }

    TaskWrapper TryDequeue()
    {
      auto lock = std::unique_lock<std::mutex>{ queue_access_, std::defer_lock };
      if (lock.try_lock())
      {
        if (queue_.empty())
        {
          return TaskWrapper{};
        }
        auto result = std::move(queue_.front());
        queue_.pop();
        return result;
      }
      return TaskWrapper{};
    }

  size_t Size() const
  {
    auto lock = std::unique_lock<std::mutex>{ queue_access_ };
    return queue_.size();
  }

  private:
    std::queue<TaskWrapper> queue_;
    mutable std::mutex queue_access_;
  };

}; // namespace catbus
