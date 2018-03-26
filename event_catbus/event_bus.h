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
#include <functional>
#include <mutex>
#include <condition_variable>

namespace catbus {

/// Worker unit incapsulates task queue and processing thread.
class WorkerUnit
{
public:
  WorkerUnit()
  {
    thread_ = std::thread(
      [ &queue_access = queue_access_,
        &task_queue = queue_,
        &stop = stop_,
        &activity = queue_event_]
      () 
      {
        auto queue_lock = std::unique_lock<std::mutex>( queue_access, std::defer_lock );
        while ( !stop )
        {
          queue_lock.lock();
          while( task_queue.empty() && !stop )
          {
            activity.wait( queue_lock );
          }
          if ( !stop  )
          {
            auto task = std::move( task_queue.front() );
            task_queue.pop();
            queue_lock.unlock();
            task();
          }
        }
      } );
  }

  ~WorkerUnit()
  {
    stop_ = true;
    queue_event_.notify_all();
    if ( thread_.joinable() )
    {
      try
      {
        thread_.join();
      }
      catch( std::system_error e )
      { }
    }
  }

  void PushTask( std::function<void()> task )
  {
    std::lock_guard<std::mutex> queue_guard(queue_access_);
    queue_.push( std::move( task ) );
    queue_event_.notify_one();
  }

  WorkerUnit( const WorkerUnit& other ) = delete;
  WorkerUnit& operator =( const WorkerUnit& other ) = delete;
  WorkerUnit( WorkerUnit&& other ) = delete;
  WorkerUnit& operator =( WorkerUnit&& other ) = delete;

private:
  std::thread thread_;
  std::queue<std::function<void()>> queue_;
  std::mutex queue_access_;
  std::condition_variable queue_event_;
  bool stop_{};
};


/// Contains one or more WorkerUnits and schedules tasks between them.

/// Main scheduling principle - tasks for consumer with id will be scheduled to
/// the same worker based on id to prevent reordering.
class EventCatbus
{
public:
  EventCatbus( size_t pool_size = 0 )
  {
    size_t worker_count = pool_size ? pool_size : std::thread::hardware_concurrency();
    workers_ = new WorkerUnit[worker_count];
    pool_size_ = worker_count;
  }

  ~EventCatbus()
  {
    if ( workers_ )
    {
      delete[] workers_;
    }
  }

  /// Schedules work by simple round-robin algorithm.
  void Send(std::function<void()> task)
  {
    workers_[++dispatch_counter_ % pool_size_].PushTask( std::move( task ) );
  }

  /// Schedules work basing on provided id, tasks for same id will always execute on the same thread.
  void Send( size_t agent_id, std::function<void()> task )
  {
    workers_[agent_id % pool_size_].PushTask( std::move(task) );
  }

  EventCatbus(const EventCatbus& other) = delete;
  EventCatbus(EventCatbus&& other) = delete;
  EventCatbus& operator=(const EventCatbus& other) = delete;
  EventCatbus& operator=(EventCatbus&& other) = delete;

private:
  size_t pool_size_{};
  size_t dispatch_counter_{};
  WorkerUnit* workers_;
};

}; // namespace catbus
