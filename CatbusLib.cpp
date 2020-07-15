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

#ifdef _WIN32
#include "stdafx.h"
#endif

#include "dispatch_utils.h"
#include "worker_mutex.h"
#include "worker_lock_free.h"
#include "event_bus.h"
#include "event_sender.h"

#include <cassert>
#include <iostream>

using namespace catbus;
using namespace std::chrono_literals;

// TEST EVENTS
// constructors specified only for testing

struct Event_NoTarget
{
  Event_NoTarget() = default;
  Event_NoTarget(Event_NoTarget&&) = default;
  Event_NoTarget& operator=(Event_NoTarget&&) = default;
  // No copies made in dispatch process. 
  Event_NoTarget(const Event_NoTarget&) { assert(false); } // MSVC needs it, but it should be never called
  Event_NoTarget& operator=(const Event_NoTarget&) = delete;
};

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

struct Event_BlockerNoTarget
{
  Event_BlockerNoTarget() = default;
  Event_BlockerNoTarget(Event_BlockerNoTarget&&) = default;
  Event_BlockerNoTarget& operator=(Event_BlockerNoTarget&&) = default;
  // No copies made in dispatch process.
  Event_BlockerNoTarget(const Event_BlockerNoTarget&) { assert(false); } // MSVC needs it, but it should be never called
  Event_BlockerNoTarget& operator=(const Event_BlockerNoTarget&) = delete;
};

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

class Consumer_NoId_Waits_NoTargetEvt : public EventSender<>
{
public:
  Consumer_NoId_Waits_NoTargetEvt() = default;
  Consumer_NoId_Waits_NoTargetEvt(const Consumer_NoId_Waits_NoTargetEvt&) = delete;
  Consumer_NoId_Waits_NoTargetEvt(Consumer_NoId_Waits_NoTargetEvt&&) = delete;

  int no_target_evt_handled{ 0 };
  int blocker_received{ 0 };

  void Handle(Event_NoTarget ev)
  {
    ++no_target_evt_handled;
  }

  void Handle(Event_BlockerNoTarget ev)
  {
    ++blocker_received;
  }
};

class Consumer_NoId_Waits_TargetEvt
{
public:
  Consumer_NoId_Waits_TargetEvt() = default;
  Consumer_NoId_Waits_TargetEvt(const Consumer_NoId_Waits_TargetEvt&) = delete;
  Consumer_NoId_Waits_TargetEvt(Consumer_NoId_Waits_TargetEvt&&) = delete;

  int target_evt_handled{ 0 };

  // In fact, it can never work, events with target can only be dispatched to consumers with id.
  void Handle(Event_WithTarget ev)
  {
    ++target_evt_handled;
  }
};

class Consumer_Id_Affinity_Waits_TargetEvt : public EventSender<>
{
public:
  Consumer_Id_Affinity_Waits_TargetEvt(size_t id, size_t affinity)
    : id_{ id }, affinity_{ affinity }
  {}
  Consumer_Id_Affinity_Waits_TargetEvt(const Consumer_Id_Affinity_Waits_TargetEvt&) = delete;
  Consumer_Id_Affinity_Waits_TargetEvt(Consumer_Id_Affinity_Waits_TargetEvt&&) = delete;

  const size_t id_;
  const size_t affinity_;
  int target_evt_handled{ 0 };
  int blocker_received{ 0 };

  void Handle(Event_WithTarget ev)
  {
    ++target_evt_handled;
  }

  void Handle(Event_BlockerWithTarget ev)
  {
    ++blocker_received;
    std::this_thread::sleep_for(500ms);
  }
};

class Consumer_Id_Waits_NoTargetEvt
{
public:
  Consumer_Id_Waits_NoTargetEvt(size_t id) : id_{ id } {}
  Consumer_Id_Waits_NoTargetEvt(const Consumer_Id_Waits_NoTargetEvt&) = delete;
  Consumer_Id_Waits_NoTargetEvt(Consumer_Id_Waits_NoTargetEvt&&) = delete;

  const size_t id_;
  int no_target_evt_handled{ 0 };

  void Handle(Event_NoTarget ev)
  {
    ++no_target_evt_handled;
  }
};

class Consumer_FromFactory
{
public:
  Consumer_FromFactory() = default;
  Consumer_FromFactory(const Consumer_FromFactory&) = delete;
  Consumer_FromFactory(Consumer_FromFactory&&) = delete;

  int event_handled{ 0 };

  void Handle(Event_NoTarget ev)
  {
    ++event_handled;
  }
};

class Producer : public EventSender<Event_BlockerNoTarget, Event_BlockerWithTarget, Event_NoTarget, Event_WithTarget>
{
public:
  Producer() = default;
  Producer(const Consumer_FromFactory&) = delete;
  Producer(Consumer_FromFactory&&) = delete;

  int event_handled{ 0 };

  void Handle(Event_InitProducer ev)
  {
    if (ev.data == 0)
    {
      Send(Event_BlockerNoTarget{});
      Send(Event_NoTarget{});
    }
    else
    {
      Send(Event_BlockerWithTarget{ ev.data });
      Send(Event_WithTarget{ ev.data });
    }
  }
};

// TEST FUNCTIONS

/// Used for events without "size_t target" field.
bool BasicStaticDispatch()
{
  EventCatbus<WorkerUnitMutex> catbus{ 1 };
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
  static_dispatch(catbus, Event_NoTarget{}, B, A);
  std::this_thread::sleep_for(100ms);
  return ok = A.no_target_evt_handled == 1 && B.target_evt_handled == 0;
}

/// Used for events with "size_t target" field.
bool BasicDynamicDispatch()
{
  EventCatbus<WorkerUnitMutex> catbus{ 1 };
  Consumer_Id_Affinity_Waits_TargetEvt A{ 1, 0 };
  Consumer_Id_Affinity_Waits_TargetEvt B{ 2, 1 };

  bool ok = has_id<Consumer_Id_Affinity_Waits_TargetEvt>::value;
  if (!ok)
  {
    return false;
  }
  ok = has_handler<Consumer_Id_Affinity_Waits_TargetEvt, Event_WithTarget>::value;
  if (!ok)
  {
    return false;
  }
  dynamic_dispatch(catbus, Event_WithTarget{ 1 }, A, B);
  std::this_thread::sleep_for(100ms);
  return ok =  A.target_evt_handled == 1 && B.target_evt_handled == 0;
}

/// If candidate with proper id does not have handler for the event, exception should be thrown.
bool FailedDynDispatchNoHandler()
{
  EventCatbus<WorkerUnitMutex> catbus{ 1 };
  Consumer_Id_Affinity_Waits_TargetEvt A{ 1, 0 };
  Consumer_Id_Waits_NoTargetEvt B{ 2 };

  bool ok = has_id<Consumer_Id_Affinity_Waits_TargetEvt>::value
    && has_id<Consumer_Id_Waits_NoTargetEvt>::value;
  if (!ok)
  {
    return false;
  }
  ok = has_handler<Consumer_Id_Affinity_Waits_TargetEvt, Event_WithTarget>::value
    && !has_handler<Consumer_Id_Waits_NoTargetEvt, Event_WithTarget>::value;
  if (!ok)
  {
    return false;
  }

  bool exception_caught{};
  try
  {
    dynamic_dispatch(catbus, Event_WithTarget{ 2 }, A, B);
  }
  catch (dispatch_error&)
  {
    exception_caught = true;
  }
  return exception_caught;
}

/// If all candidates have proper handlers, but wrong ids, exception should be thrown.
bool FailedDynDispatchNoId()
{
  EventCatbus<WorkerUnitMutex> catbus{ 1 };
  Consumer_Id_Affinity_Waits_TargetEvt A{ 2, 0 };
  Consumer_Id_Affinity_Waits_TargetEvt B{ 1, 1 };

  bool exception_caught{};
  try
  {
    dynamic_dispatch(catbus, Event_WithTarget{ 3 }, A, B);
  }
  catch (dispatch_error&)
  {
    exception_caught = true;
  }
  return exception_caught;
}

/// This scheduling used for consumers without "const size_t id_" member.

bool RoundRobinScheduling()
{
  EventCatbus<WorkerUnitLockFree> catbus{ 2 };
  Consumer_NoId_Waits_NoTargetEvt A;
  Producer P;
  setup_dispatch(catbus, A, P);
  static_dispatch(catbus, Event_InitProducer{ 0 }, P);

  std::this_thread::sleep_for(100ms);

  bool ok = A.blocker_received == 1 && A.no_target_evt_handled == 1;
  return ok;
}

/// Used for consumers, who has "const size_t id_" member.

bool OrderedScheduling()
{
  EventCatbus<WorkerUnitLockFree> catbus{ 2 };
  Consumer_Id_Affinity_Waits_TargetEvt A{ 1, 0 };
  // Consumer B is needed because Producer can send events without target, which will be statically
  // dispatched, and compilation will fail if there would be no handlers.
  Consumer_NoId_Waits_NoTargetEvt B;
  Producer P;
  setup_dispatch(catbus, A, B, P);
  static_dispatch(catbus, Event_InitProducer{ 1 }, P);

  std::this_thread::sleep_for(100ms);

  bool ok = A.blocker_received == 1 && A.target_evt_handled == 0;
  return ok;
}

/// Send task directly to event bus, using helper function 'make_handle_task'
/// with custom factory function.
bool NoRoutingTask()
{
  EventCatbus<WorkerUnitLockFree> catbus{ 1 };
  Consumer_FromFactory agent;
  catbus.Send(
    make_handle_task(
      Event_NoTarget{},
      [agent_ptr = &agent]() { return agent_ptr; }
  ));
  std::this_thread::sleep_for(100ms);
  bool ok = agent.event_handled == 1;
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
  std::cout << "Dynamic dispatch fail because id not found: " << (passed ? "PASS\n" : "FAIL\n");

  passed = RoundRobinScheduling();
  std::cout << "Round robin scheduling: " << (passed ? "PASS\n" : "FAIL\n");

  passed = OrderedScheduling();
  std::cout << "Ordered scheduling: " << (passed ? "PASS\n" : "FAIL\n");

  passed = NoRoutingTask();
  std::cout << "Send task without routing: " << (passed ? "PASS\n" : "FAIL\n");
  
  return passed ? 0 : 1;
}

