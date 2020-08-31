#include "dispatch_utils.h"
#include "event_bus.h"
#include "event_sender.h"
#include "queue_mutex.h"
#include "queue_lock_free.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

using time_type = std::chrono::time_point<std::chrono::system_clock>;
using interval_type = std::chrono::duration<size_t, std::micro>;

// --------------------------------------------------

struct Small_NoTarget {
    time_type created_ts;
    int data1;
};

struct Medium_NoTarget {
    time_type created_ts;
    long data1;
    long data2;
    long data3;
    long data4;
    long data5;
    long data6;
    long data7;
    long data8;
    long data9;
    long data0;
};

struct LongWait_NoTarget {
    time_type created_ts;
    interval_type to_sleep{500000};
};

struct Small_WithTarget {
    size_t target;
    time_type created_ts;
    int data1;
};

struct Medium_WithTarget {
    size_t target;
    time_type created_ts;
    long data1;
    long data2;
    long data3;
    long data4;
    long data5;
    long data6;
    long data7;
    long data8;
    long data9;
    long data0;
};

// --------------------------------------------------

class SmallEvtConsumer : public catbus::EventSender<Medium_NoTarget, Small_NoTarget>
{
public:
    std::atomic_long max_time_{0};
    std::atomic_long counter_{0};

    void Handle(Small_NoTarget evt)
    {
        time_type now = std::chrono::high_resolution_clock::now();
        auto waiting_time = interval_type{
            std::chrono::duration_cast<std::chrono::duration<size_t, std::micro>>(now - evt.created_ts)};
        if (waiting_time.count() > max_time_)
        {
            max_time_ = waiting_time.count();
        }
        counter_.fetch_add(1, std::memory_order_relaxed);
        if((counter_ & 255) != 0) {
            Send(Small_NoTarget{now, 42});
        }
        else {
            Send(Medium_NoTarget{now, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0});
        }
    }
};

class MediumEvtConsumer : public catbus::EventSender<Small_NoTarget, LongWait_NoTarget>
{
public:
    std::atomic_long max_time_{0};
    std::atomic_long counter_{0};

    void Handle(Medium_NoTarget evt)
    {
        time_type now = std::chrono::high_resolution_clock::now();
        auto waiting_time = interval_type{
            std::chrono::duration_cast<std::chrono::duration<size_t, std::micro>>(now - evt.created_ts)};
        if (waiting_time.count() > max_time_)
        {
            max_time_ = waiting_time.count();
        }
        counter_.fetch_add(1, std::memory_order_relaxed);
        if((counter_ & 255) != 0) {
            Send(Small_NoTarget{now, 42});
        }
        else {
            Send(LongWait_NoTarget{now});
        }
    }
};

class LongEvtConsumer : public catbus::EventSender<>
{
public:
    std::atomic_long max_time_{0};
    std::atomic_long counter_{0};

    void Handle(LongWait_NoTarget evt)
    {
        time_type now = std::chrono::high_resolution_clock::now();
        auto waiting_time = interval_type{
            std::chrono::duration_cast<std::chrono::duration<size_t, std::micro>>(now - evt.created_ts)};
        if (waiting_time.count() > max_time_)
        {
            max_time_ = waiting_time.count();
        }
        counter_.fetch_add(1, std::memory_order_relaxed);
        //std::this_thread::sleep_for(evt.to_sleep);
    }
};

class TargetedEventsConsumer : public catbus::EventSender<Small_WithTarget, Medium_WithTarget> 
{
public:
    const size_t id_;
    std::atomic_long max_time_{0};
    std::atomic_ulong counter_{0};

    explicit TargetedEventsConsumer(size_t id) : id_{id} {}

    void Handle(Small_WithTarget evt)
    {
        time_type now = std::chrono::high_resolution_clock::now();
        auto waiting_time = interval_type{
            std::chrono::duration_cast<std::chrono::duration<size_t, std::micro>>(now - evt.created_ts)};
        if (waiting_time.count() > max_time_)
        {
            max_time_ = waiting_time.count();
        }
        auto count = counter_.fetch_add(1, std::memory_order_relaxed);
        if((count & 255) != 0) {
            Send(Small_WithTarget{count % 3, now, 42});
        }
        else {
            Send(Medium_WithTarget{count % 3, now, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0});
        }
    }

    void Handle(Medium_WithTarget evt)
    {
        time_type now = std::chrono::high_resolution_clock::now();
        auto waiting_time = interval_type{
            std::chrono::duration_cast<std::chrono::duration<size_t, std::micro>>(now - evt.created_ts)};
        if (waiting_time.count() > max_time_)
        {
            max_time_ = waiting_time.count();
        }
        auto count = counter_.fetch_add(1, std::memory_order_relaxed);

        Send(Small_WithTarget{count % 3, now, 42});
    }
};

// --------------------------------------------------

catbus::EventCatbus<catbus::SimpleLockFreeQueue<65536>, 15, 15> bus;
//catbus::EventCatbus<catbus::MutexProtectedQueue, 15, 15> bus;

int main(int argc, char** argv) {
    SmallEvtConsumer A;
    MediumEvtConsumer B;
    LongEvtConsumer C;
    //TargetedEventsConsumer AT{0}, BT{1}, CT{2};

    catbus::setup_dispatch(bus, A, B, C);
    for(size_t i = 0; i < 1000; ++i) {
        catbus::static_dispatch(bus, Small_NoTarget{std::chrono::high_resolution_clock::now(), 42}, A, B, C);
    }
    auto begin = std::chrono::high_resolution_clock::now();
    while(A.counter_ + B.counter_ + C.counter_ < 50000000) {
        std::this_thread::sleep_for(interval_type{200000});
        std::cout << "## Count A: " << A.counter_ << "; count B: " << B.counter_ << "; count C: " << C.counter_ << "\n";
        auto sizes = bus.QueueSizes();
        std::cout << "## [";
        for(auto size: sizes) {
            std::cout << size << ",";
        }
        std::cout << "]\n\n";
    }
    bus.Stop();
    auto end = std::chrono::high_resolution_clock::now();
    auto count = A.counter_.load(std::memory_order_relaxed);
    auto countB = B.counter_.load(std::memory_order_relaxed);
    auto countC = C.counter_.load(std::memory_order_relaxed);
    auto elapsed_seconds =
      std::chrono::duration_cast<std::chrono::duration<double>>(end - begin);
    std::cout << "## Time to process 10 000 000 events: " << elapsed_seconds.count() << "s\n";
    std::cout << "## Avg. request/second: " << (double)(count + countB + countC)/elapsed_seconds.count() << "\n";
    std::cout << "## Max waiting time A: " << A.max_time_ << "mcs\n";
    std::cout << "## Max waiting time B: " << B.max_time_ << "mcs\n";
    std::cout << "## Max waiting time C: " << C.max_time_ << "mcs\n";
}
