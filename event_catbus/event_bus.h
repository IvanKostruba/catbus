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

#include "task_wrapper.h"

#include <array>
#include <atomic>
#include <functional>
#include <system_error>
#include <thread>

namespace catbus {

// Incapsulates worker threads and queues and enqueues tasks.
// The Queue type must be thread-safe.

template<typename Queue, size_t NQ, size_t NWrk>
class EventCatbus {
    static_assert(NQ >= 1, "At least one queue is needed to run dispatching.");
    static_assert(NWrk >= 1, "At least one worker thread is needed to handle events.");
public:
    EventCatbus() {
        for(size_t i = 0; i < NWrk; ++i) {
            workers_[i].setup(&queues_, i);
        }
    }

    ~EventCatbus() {
        stop();
    }

    void stop() {
        for (auto& worker : workers_) {
            worker.stop_ = true;
        }
    }

    // Enqueues tasks to specified queue, falls back to simple round-robin algorithm if
    // provided value is out of range.
    void send(TaskWrapper task, size_t q) {
        // std::move is used throughout the library and here as well to avoid copying of events,
        // this is why it's hard to implement try_enqueue() so we are risking some waiting here.
        if (q < NQ) {
            queues_[q].enqueue(std::move(task));
        } else {
            queues_[dispatch_counter_.fetch_add(1, std::memory_order_relaxed) % NQ].
                enqueue(std::move(task));
        }
    }

    std::array<size_t, NQ> QueueSizes() const {
        std::array<size_t, NQ> result;
        for(size_t i = 0; i < NQ; ++i) {
            result[i] = queues_[i].size();
        }
        return result;
    }

    // TODO: (ideas) potentially there can be special 'high piority' queue with separate workers.
    // Also send() method with explicit queue idx might be useful. For example, when there are more
    // queues than workers, additional queues will be visited when workers will have free time, so
    // it can be sort of priority mechanism as well.

    EventCatbus(const EventCatbus& other) = delete;
    EventCatbus(EventCatbus&& other) = delete;
    EventCatbus& operator=(const EventCatbus& other) = delete;
    EventCatbus& operator=(EventCatbus&& other) = delete;

private:
    struct Worker {

        void setup(std::array<Queue, NQ>* queues, size_t primary) {
            thread_ = std::thread(
                [&queues = *queues, primary = primary, &stop = stop_] () {
                    while (!stop) {
                        for(size_t i = primary; !stop && i < primary + NQ; ++i) {
                            auto task = queues[i % NQ].try_dequeue();
                            if (task.is_valid()) {
                                // Passing primary queue idx, because worker will check it on the
                                // next iteration anyway. 
                                task.run(primary);
                                break;
                            }
                        }
                    }
                }
            );
        }

        ~Worker() {
            if (thread_.joinable()) {
                try {
                    thread_.join();
                }
                catch (std::system_error e) {
                }
            }
        }

        std::thread thread_;
        bool stop_{};
    };

    std::atomic_uint dispatch_counter_{};
    std::array<Worker, NWrk> workers_;
    std::array<Queue, NQ> queues_;
};

}; // namespace catbus
