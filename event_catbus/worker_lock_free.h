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

#include "exception.h"

#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

namespace catbus {

  /// Worker unit incapsulates task queue and processing thread.
  /*!
  *  Queue here is a lock-free ring buffer. Exception is thrown in case of overflow.
  *  This might be useful when system has millions of fast-processing event per second.
  */
  class WorkerUnitLockFree
  {
  public:
    WorkerUnitLockFree()
    {
      buffer_ = new Task[buffer_size_];
      thread_ = std::thread(
        [&task_queue = buffer_,
        &stop = stop_,
        &consumed = consumed_,
        mask = mask_]
      ()
      {
        while (!stop)
        {
          unsigned current = consumed.fetch_add(1, std::memory_order_relaxed) & mask;
          while (!task_queue[current].ready.load(std::memory_order_acquire) && !stop)
          {
            std::this_thread::yield();
          }
          if (stop)
          {
            return;
          }
          std::function<void()> task = std::move(task_queue[current].run);
          task_queue[current].ready.store(false, std::memory_order_release);
          task();
        }
      });
    }

    ~WorkerUnitLockFree()
    {
      stop_ = true;
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

    void PushTask(std::function<void()> task) noexcept(false)
    {
      unsigned prod = produced_.fetch_add(1, std::memory_order_relaxed) & mask_;
      while (buffer_[prod].ready.load(std::memory_order_acquire) && !stop_)
      {
        throw queue_overflow{};
        //std::this_thread::yield();
      }
      if (stop_)
      {
        return;
      }
      buffer_[prod].run = std::move(task);
      buffer_[prod].ready.store(true, std::memory_order_release);
    }

    WorkerUnitLockFree(const WorkerUnitLockFree& other) = delete;
    WorkerUnitLockFree& operator =(const WorkerUnitLockFree& other) = delete;
    WorkerUnitLockFree(WorkerUnitLockFree&& other) = delete;
    WorkerUnitLockFree& operator =(WorkerUnitLockFree&& other) = delete;

  private:
    struct Task
    {
      std::atomic_bool ready{ false };
      std::function<void()> run;
    };
    Task* buffer_;
    static const size_t buffer_size_{ 4096 };
    static const size_t mask_{ buffer_size_ - 1 };

    std::atomic_uint consumed_{ 0 };
    std::atomic_uint produced_{ 0 };
    std::thread thread_;
    bool stop_{};
  };

}; // catbus
