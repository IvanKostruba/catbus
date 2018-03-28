/******************************************************************************
MIT License

Copyright(c) 2018 IvanKostruba

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
******************************************************************************/

#pragma once

#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace catbus {

  /// Worker unit incapsulates task queue and processing thread.
  /*!
  *  Queue access is synchronized with mutex. This is basic version,
  *  probably suitable for the majority of cases.
  */
  class WorkerUnitMutex
  {
  public:
    WorkerUnitMutex()
    {
      thread_ = std::thread(
        [&queue_access = queue_access_,
        &task_queue = queue_,
        &stop = stop_,
        &activity = queue_event_]
      ()
      {
        auto queue_lock = std::unique_lock<std::mutex>(queue_access, std::defer_lock);
        while (!stop)
        {
          queue_lock.lock();
          while (task_queue.empty() && !stop)
          {
            activity.wait(queue_lock);
          }
          if (!stop)
          {
            auto task = std::move(task_queue.front());
            task_queue.pop();
            queue_lock.unlock();
            task();
          }
        }
      });
    }

    ~WorkerUnitMutex()
    {
      stop_ = true;
      queue_event_.notify_all();
      if (thread_.joinable())
      {
        try
        {
          thread_.join();
        }
        catch (std::system_error e)
        {
        }
      }
    }

    void PushTask(std::function<void()> task)
    {
      std::lock_guard<std::mutex> queue_guard(queue_access_);
      queue_.push(std::move(task));
      queue_event_.notify_one();
    }

    WorkerUnitMutex(const WorkerUnitMutex& other) = delete;
    WorkerUnitMutex& operator =(const WorkerUnitMutex& other) = delete;
    WorkerUnitMutex(WorkerUnitMutex&& other) = delete;
    WorkerUnitMutex& operator =(WorkerUnitMutex&& other) = delete;

  private:
    std::thread thread_;
    std::queue<std::function<void()>> queue_;
    std::mutex queue_access_;
    std::condition_variable queue_event_;
    bool stop_{};
  };

}; // catbus
