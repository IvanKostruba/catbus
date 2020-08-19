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
#include <system_error>
#include <thread>

#include <iostream>

namespace catbus {

/// Contains one or more worker threads and queues and schedules tasks between them.

template<typename Queue, size_t NQ = 1, size_t NW = 1>
class EventCatbus
{
public:
  EventCatbus()
  {
    for(size_t i = 0; i < workers_.size(); ++i)
    {
      workers_[i].Setup(&queues_, i);
    }
  }

  /// Schedules work by simple round-robin algorithm.
  void Send(std::function<void()> task)
  {
    std::cout << "Sent to " << ((dispatch_counter_ + 1) % queues_.size()) << "\n";
    queues_[++dispatch_counter_ % queues_.size()].Enqueue( std::move( task ) );
  }

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
            for(size_t i = primary; i < primary + NQ; ++i)
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

    std::thread thread_;
    std::array<Queue, NQ>* queues_;
    size_t primary_;
    bool stop_{};
  };

  size_t dispatch_counter_{};
  std::array<Worker, NW> workers_;
  std::array<Queue, NQ> queues_;
};

}; // namespace catbus
