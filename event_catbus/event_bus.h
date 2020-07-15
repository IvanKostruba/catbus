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

#include <functional>
#include <thread>

namespace catbus {

/// Contains one or more WorkerUnits and schedules tasks between them.

/// Main scheduling principle - tasks for consumer with id will be scheduled to
/// the same worker based on id to prevent reordering.
// TODO: add thread monitoring option, so that thread occupied for too long would
// be replaced with another one, and have it's queue transferred to the new thread.
template<typename Worker>
class EventCatbus
{
public:
  explicit EventCatbus( size_t pool_size = 0 )
  {
    size_t worker_count = pool_size ? pool_size : std::thread::hardware_concurrency();
    workers_ = new Worker[worker_count];
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
  Worker* workers_;
};

}; // namespace catbus
