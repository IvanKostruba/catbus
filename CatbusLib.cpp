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

// DispatchLib.cpp : Defines the entry point for the console application.
//

#include "dispatch_utils.h"
#include "event_bus.h"
#include "event_sender.h"
#include "queue_mutex.h"
#include "queue_lock_free.h"

#include <cassert>
#include <iostream>
#include <thread>

using namespace catbus;
using namespace std::chrono_literals;

// TEST EVENTS

// Event without 'target' field is used to test static dispatch.
struct Event_NoTarget
{
  Event_NoTarget() = default;
  Event_NoTarget(Event_NoTarget&&) = default;
  Event_NoTarget& operator=(Event_NoTarget&&) = default;
  // No copies made in dispatch process. 
  Event_NoTarget(const Event_NoTarget&) { assert(false); } // MSVC needs it, but it should be never called
  Event_NoTarget& operator=(const Event_NoTarget&) = delete;
};

// Event with 'target' is dispatched dynamically by comparing with counsumer 'id' field.
struct Event_WithTarget
{
  Event_WithTarget(size_t id) : target{id} {}
  Event_WithTarget(Event_WithTarget&&) = default;
  Event_WithTarget& operator=(Event_WithTarget&&) = default;
  // No copies made in dispatch process.
  Event_WithTarget(const Event_WithTarget&) { assert(false); } // MSVC needs it, but it should be never called
  Event_WithTarget& operator=(const Event_WithTarget&) = delete;

  size_t target;
};

// Processing of this events in test consumers triggers sleep, imitating some long operation.
struct Event_BlockerWithTarget
{
  Event_BlockerWithTarget(size_t id) : target{ id } {}
  Event_BlockerWithTarget(Event_BlockerWithTarget&&) = default;
  Event_BlockerWithTarget& operator=(Event_BlockerWithTarget&&) = default;
  // No copies made in dispatch process.
  Event_BlockerWithTarget(const Event_BlockerWithTarget&) { assert(false); } // MSVC needs it, but it should be never called
  Event_BlockerWithTarget& operator=(const Event_BlockerWithTarget&) = delete;

  size_t target;
};

// Processing of this events in test consumers triggers sleep, imitating some long operation.
struct Event_BlockerNoTarget
{
  Event_BlockerNoTarget() = default;
  Event_BlockerNoTarget(Event_BlockerNoTarget&&) = default;
  Event_BlockerNoTarget& operator=(Event_BlockerNoTarget&&) = default;
  // No copies made in dispatch process.
  Event_BlockerNoTarget(const Event_BlockerNoTarget&) { assert(false); } // MSVC needs it, but it should be never called
  Event_BlockerNoTarget& operator=(const Event_BlockerNoTarget&) = delete;
};

// Initialize event producer that in turn will send some other events.
struct Event_InitProducer
{
  explicit Event_InitProducer(size_t i) : data{ i } {}
  Event_InitProducer(Event_InitProducer&&) = default;
  Event_InitProducer& operator=(Event_InitProducer&&) = default;
  // No copies made in dispatch process.
  Event_InitProducer(const Event_InitProducer&) { assert(false); } // MSVC needs it, but it should be never called
  Event_InitProducer& operator=(const Event_InitProducer&) = delete;

  size_t data;
};

// TEST CONSUMERS

// Used to test static dispatching of events, based on event type and handler method signature.
class Consumer_NoId_Waits_NoTargetEvt
{
public:
  Consumer_NoId_Waits_NoTargetEvt() = default;
  Consumer_NoId_Waits_NoTargetEvt(const Consumer_NoId_Waits_NoTargetEvt&) = delete;
  Consumer_NoId_Waits_NoTargetEvt(Consumer_NoId_Waits_NoTargetEvt&&) = delete;

  int no_target_evt_handled{ 0 };
  int blocker_received{ 0 };

  void handle(Event_NoTarget ev, size_t)
  {
    ++no_target_evt_handled;
  }

  void handle(Event_BlockerNoTarget ev, size_t)
  {
    ++blocker_received;
    std::this_thread::sleep_for(500ms);
  }
};

// Deliberately broken consumer, used to test exceptions on failed dispatch.
class Consumer_NoId_Waits_TargetEvt
{
public:
  Consumer_NoId_Waits_TargetEvt() = default;
  Consumer_NoId_Waits_TargetEvt(const Consumer_NoId_Waits_TargetEvt&) = delete;
  Consumer_NoId_Waits_TargetEvt(Consumer_NoId_Waits_TargetEvt&&) = delete;

  int target_evt_handled{ 0 };

  // Even though it has handler, events with target can only be dispatched to consumers with id_.
  void handle(Event_WithTarget ev, size_t)
  {
    ++target_evt_handled;
  }
};

// Proper consumer for targeted events, used to test positive scenarios.
class Consumer_Id_Waits_TargetEvt
{
public:
  explicit Consumer_Id_Waits_TargetEvt(size_t id)
    : id_{ id }
  {}
  Consumer_Id_Waits_TargetEvt(const Consumer_Id_Waits_TargetEvt&) = delete;
  Consumer_Id_Waits_TargetEvt(Consumer_Id_Waits_TargetEvt&&) = delete;

  const size_t id_;
  int target_evt_handled{ 0 };
  int blocker_received{ 0 };

  void handle(Event_WithTarget ev, size_t)
  {
    ++target_evt_handled;
  }

  void handle(Event_BlockerWithTarget ev, size_t)
  {
    ++blocker_received;
    std::this_thread::sleep_for(500ms);
  }
};

// This consumer has id_ for only reason - to test another dispatch failure when event has target,
// and dispatch function could find consumer by id_, but it lacks proper handler.
class Consumer_Id_Waits_NoTargetEvt
{
public:
  Consumer_Id_Waits_NoTargetEvt(size_t id) : id_{ id } {}
  Consumer_Id_Waits_NoTargetEvt(const Consumer_Id_Waits_NoTargetEvt&) = delete;
  Consumer_Id_Waits_NoTargetEvt(Consumer_Id_Waits_NoTargetEvt&&) = delete;

  const size_t id_;
  int no_target_evt_handled{ 0 };

  void handle(Event_NoTarget ev, size_t)
  {
    ++no_target_evt_handled;
  }
};

// Event producer is used to test automatic setup of event sender methods.
class Producer
{
public:
  Producer() = default;
  Producer(const Producer&) = delete;
  Producer(Producer&&) = delete;

  EventSender<
    Event_BlockerNoTarget, Event_BlockerWithTarget, Event_NoTarget, Event_WithTarget
  > sender_;
  int event_handled{ 0 };

  void handle(Event_InitProducer ev, size_t q)
  {
    if (ev.data == 0)
    {
      // Passing the same queue idx to send() method to keep processing local.
      sender_.send(Event_BlockerNoTarget{}, q);
      sender_.send(Event_NoTarget{}, q);
      sender_.send(Event_NoTarget{}, q);
    }
    else
    {
      sender_.send(Event_BlockerWithTarget{ ev.data }, q);
      std::this_thread::sleep_for(50ms); // See comment for NestedBusScheduling().
      sender_.send(Event_WithTarget{ ev.data }, q);
    }
  }
};

// This consumer forwards the events to another bus, that dispatches them to the final consumer.
// This second bus has only one thread, so it processes events in the same order it received them
// though of course it can be different from the order in which they were produced, because events
// potentially travel through several different queues and served by different threads. But if
// events are produced with big enough time gap, it's enough to guarantee correct sequence.
class OrderedEventsProcessor
{
public:
  explicit OrderedEventsProcessor(size_t id)
    : id_{ id }
  {}
  OrderedEventsProcessor(const OrderedEventsProcessor&) = delete;
  OrderedEventsProcessor(OrderedEventsProcessor&&) = delete;

  const size_t id_;
  int target_evt_handled{ 0 };
  int blocker_received{ 0 };
  Consumer_Id_Waits_TargetEvt final_consumer_{ 1 };
  EventCatbus<MutexProtectedQueue, 1, 1> processor_;

  void handle(Event_WithTarget ev, size_t)
  {
    dynamic_dispatch(processor_, ROUND_ROBIN, std::move(ev), final_consumer_);
  }

  void handle(Event_BlockerWithTarget ev, size_t)
  {
    dynamic_dispatch(processor_, ROUND_ROBIN, std::move(ev), final_consumer_);
  }
};

// TEST FUNCTIONS

// Static dispatch is used for events without 'target' field. Type of event and signatures of
// potential handler methods are compared.
bool BasicStaticDispatch()
{
  EventCatbus<MutexProtectedQueue, 1, 1> catbus;
  Consumer_NoId_Waits_NoTargetEvt A;
  Consumer_NoId_Waits_TargetEvt B;

  bool ok = !has_id<Consumer_NoId_Waits_NoTargetEvt>::value && !has_id<Consumer_NoId_Waits_TargetEvt>::value;
  if (!ok)
  {
    return false;
  }
  ok = has_handler<Consumer_NoId_Waits_NoTargetEvt, Event_NoTarget>::value && !has_handler<Consumer_NoId_Waits_TargetEvt, Event_NoTarget>::value;
  if (!ok)
  {
    return false;
  }
  static_dispatch(catbus, ROUND_ROBIN, Event_NoTarget{}, B, A);
  std::this_thread::sleep_for(100ms);
  return ok = A.no_target_evt_handled == 1 && B.target_evt_handled == 0;
}

// If event has 'target' field, it is compared against 'id' field of potential consumers which
// has proper handler method for given event type.
bool BasicDynamicDispatch()
{
  EventCatbus<SimpleLockFreeQueue<16>, 1, 1> catbus;
  Consumer_Id_Waits_TargetEvt A{ 1 };
  Consumer_Id_Waits_TargetEvt B{ 2 };

  bool ok = has_id<Consumer_Id_Waits_TargetEvt>::value;
  if (!ok)
  {
    return false;
  }
  ok = has_handler<Consumer_Id_Waits_TargetEvt, Event_WithTarget>::value;
  if (!ok)
  {
    return false;
  }
  dynamic_dispatch(catbus, ROUND_ROBIN, Event_WithTarget{ 1 }, A, B);
  std::this_thread::sleep_for(100ms);
  return ok =  A.target_evt_handled == 1 && B.target_evt_handled == 0;
}

// If candidate with proper id_ does not have handler for the event, exception should be thrown.
bool FailedDynDispatchNoHandler()
{
  EventCatbus<MutexProtectedQueue, 1, 1> catbus;
  Consumer_Id_Waits_TargetEvt A{ 1 };
  Consumer_Id_Waits_NoTargetEvt B{ 2 };

  bool ok = has_id<Consumer_Id_Waits_TargetEvt>::value
    && has_id<Consumer_Id_Waits_NoTargetEvt>::value;
  if (!ok)
  {
    return false;
  }
  ok = has_handler<Consumer_Id_Waits_TargetEvt, Event_WithTarget>::value
    && !has_handler<Consumer_Id_Waits_NoTargetEvt, Event_WithTarget>::value;
  if (!ok)
  {
    return false;
  }

  bool exception_caught{};
  try
  {
    dynamic_dispatch(catbus, ROUND_ROBIN, Event_WithTarget{ 2 }, A, B);
  }
  catch (dispatch_error&)
  {
    exception_caught = true;
  }
  return exception_caught;
}

// If all candidates have proper handlers, but wrong ids, exception should be thrown.
bool FailedDynDispatchNoId()
{
  EventCatbus<SimpleLockFreeQueue<>, 1, 1> catbus;
  Consumer_Id_Waits_TargetEvt A{ 2 };
  Consumer_Id_Waits_TargetEvt B{ 1 };

  bool exception_caught{};
  try
  {
    dynamic_dispatch(catbus, ROUND_ROBIN, Event_WithTarget{ 3 }, A, B);
  }
  catch (dispatch_error&)
  {
    exception_caught = true;
  }
  return exception_caught;
}

// Event bus puts events into queues with round robin algorithm. Worker thread then checks its
// 'primary' queue and if it's empty goes to check other queues. In this test one of the threads
// is blocked by processing Event_BlockerNoTarget issued by Producer, but the other thread still
// picks up both Event_NoTarget events even though they are in different queues.
bool SchedulingAndTaskStealing()
{
  EventCatbus<SimpleLockFreeQueue<16>, 2, 2> catbus;
  Consumer_NoId_Waits_NoTargetEvt A;
  Producer P;
  setup_dispatch(catbus, A, P);
  static_dispatch(catbus, ROUND_ROBIN, Event_InitProducer{ 0 }, P);

  std::this_thread::sleep_for(100ms);

  bool ok = A.blocker_received == 1 && A.no_target_evt_handled == 2;
  return ok;
}

// Event consumers can have another bus inside them. This one intended to process events in FIFO
// order and is built by dispatching events to a separate bus with single thread.
// If that thread is blocked, all other events that were sent there will have to wait.
// This implemetation has its limits, it can change the order of events if they were issued in
// quick succession.
bool NestedBusScheduling()
{
  EventCatbus<MutexProtectedQueue, 2, 2> catbus;
  OrderedEventsProcessor O{ 1 };
  // Consumer B is needed because Producer can send events without target, which will be statically
  // dispatched, and compilation will fail if there would be no handlers.
  Consumer_NoId_Waits_NoTargetEvt B;
  Producer P;
  setup_dispatch(catbus, O, B, P);
  EventSender<Event_InitProducer> sender{catbus, P};
  sender.send(Event_InitProducer{ 1 });

  std::this_thread::sleep_for(100ms);
  
  bool ok = O.final_consumer_.blocker_received == 1 && O.final_consumer_.target_evt_handled == 0;
  return ok;
}

// ENTRY POINT

int main()
{
  bool passed{};

  passed = BasicStaticDispatch();
  std::cout << "Basic static dispatch: " << (passed ? "PASS\n" : "FAIL\n");

  passed = BasicDynamicDispatch();
  std::cout << "Basic dynamic dispatch: " << (passed ? "PASS\n" : "FAIL\n");

  passed = FailedDynDispatchNoHandler();
  std::cout << "Dynamic dispatch fail due to absent handler: " << (passed ? "PASS\n" : "FAIL\n");

  passed = FailedDynDispatchNoId();
  std::cout << "Dynamic dispatch fail because id is not found: " << (passed ? "PASS\n" : "FAIL\n");

  passed = SchedulingAndTaskStealing();
  std::cout << "Scheduling and task stealing: " << (passed ? "PASS\n" : "FAIL\n");

  passed = NestedBusScheduling();
  std::cout << "Nested bus scheduling: " << (passed ? "PASS\n" : "FAIL\n");
  
  return passed ? 0 : 1;
}
