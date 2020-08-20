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

#include <array>
#include <functional>
#include <system_error>
#include <thread>

namespace catbus {

// Incapsulates worker threads and queues and enqueues tasks.

template<typename Queue, size_t NQ, size_t NWrk>
class EventCatbus
{
  static_assert(NQ >= 1, "At least one queue is needed to run dispatching.");
  static_assert(NWrk >= 1, "At least one worker thread is needed to handle events.");
public:
  EventCatbus()
  {
    for(size_t i = 0; i < NWrk; ++i)
    {
      workers_[i].Setup(&queues_, i);
    }
  }

  ~EventCatbus()
  {
    Stop();
  }

  void Stop()
  {
    for (auto& worker : workers_)
    {
      worker.stop_ = true;
    }
  }

  // Enqueues tasks with simple round-robin algorithm.
  void Send(std::function<void()> task)
  {
    // std::move is used throughout the library and here as well to avoid copying of events,
    // this is why it's hard to implement try_enqueue() so we are risking some waiting here.
    queues_[++dispatch_counter_ % NQ].Enqueue( std::move( task ) );
  }

  // TODO: (ideas) potentially there can be special 'high piority' queue with separate workers.
  // Also Send() method with explicit queue idx might be useful. For example, when there are more
  // queues than workers, additional queues will be visited when workers will have free time, so
  // it can be sort of priority mechanism as well.

  EventCatbus(const EventCatbus& other) = delete;
  EventCatbus(EventCatbus&& other) = delete;
  EventCatbus& operator=(const EventCatbus& other) = delete;
  EventCatbus& operator=(EventCatbus&& other) = delete;

private:
  struct Worker
  {
    void Setup(std::array<Queue, NQ>* queues, size_t primary)
    {
      queues_ = queues;
      primary_ = primary;
      thread_ = std::thread(
        [&queues = queues_,
        primary = primary_,
        &stop = stop_] ()
        {
          while (!stop)
          {
            for(size_t i = primary; !stop && i < primary + NQ; ++i)
            {
              auto task = (*queues)[i % NQ].TryDequeue();
              if (task)
              {
                (*task)();
                break;
              }
            }
          }
        }
      );
    }

    ~Worker()
    {
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

    std::thread thread_;
    std::array<Queue, NQ>* queues_;
    size_t primary_;
    bool stop_{};
  };

  size_t dispatch_counter_{};
  std::array<Worker, NWrk> workers_;
  std::array<Queue, NQ> queues_;
};

}; // namespace catbus
