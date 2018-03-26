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

#include "stdafx.h"

#include "dispatch_utils.h"
#include "worker_mutex.h"
#include "worker_lock_free.h"
#include "event_bus.h"
#include "global_dispatcher.h"

#include <iostream>

using namespace catbus;
using namespace std::chrono_literals;

// TEST EVENTS
// constructors specified only for testing

struct NoTargetEvent
{
  NoTargetEvent() = default;
  NoTargetEvent(NoTargetEvent&&) = default;
  NoTargetEvent& operator=(NoTargetEvent&&) = default;

  NoTargetEvent(const NoTargetEvent&) { exit(1); } // MSVC compiler requires it, but it should never be called.
  NoTargetEvent& operator=(const NoTargetEvent&) = delete;
};

struct TargetEvent
{
  TargetEvent(size_t id) : target{id} {}
  TargetEvent(TargetEvent&&) = default;
  TargetEvent& operator=(TargetEvent&&) = default;

  TargetEvent(const TargetEvent&) { exit(1); } // MSVC compiler requires it, but it should never be called.
  TargetEvent& operator=(const TargetEvent&) = delete;

  size_t target;
};

struct BlockerEvent
{
  BlockerEvent(size_t id) : target{ id } {}
  BlockerEvent(BlockerEvent&&) = default;
  BlockerEvent& operator=(BlockerEvent&&) = default;

  BlockerEvent(const BlockerEvent&) { exit(1); } // MSVC compiler requires it, but it should never be called.
  BlockerEvent& operator=(const BlockerEvent&) = delete;

  size_t target;
};

struct NoTargetBlockerEvent
{
  NoTargetBlockerEvent() = default;
  NoTargetBlockerEvent(NoTargetBlockerEvent&&) = default;
  NoTargetBlockerEvent& operator=(NoTargetBlockerEvent&&) = default;

  NoTargetBlockerEvent(const NoTargetBlockerEvent&) { exit(1); } // MSVC compiler requires it, but it should never be called.
  NoTargetBlockerEvent& operator=(const NoTargetBlockerEvent&) = delete;
};

// TEST CONSUMERS

class NoIdConsumerNoTargetEvt
{
public:
  NoIdConsumerNoTargetEvt() = default;
  NoIdConsumerNoTargetEvt(const NoIdConsumerNoTargetEvt&) = delete;
  NoIdConsumerNoTargetEvt(NoIdConsumerNoTargetEvt&&) = delete;

  int no_target_evt_handled{ 0 };
  int blocker_received{ 0 };

  void Handle(NoTargetEvent ev)
  {
    ++no_target_evt_handled;
  }

  void Handle(NoTargetBlockerEvent ev)
  {
    ++blocker_received;
  }
};

class NoIdConsumerTargetEvt
{
public:
  NoIdConsumerTargetEvt() = default;
  NoIdConsumerTargetEvt(const NoIdConsumerTargetEvt&) = delete;
  NoIdConsumerTargetEvt(NoIdConsumerTargetEvt&&) = delete;

  int target_evt_handled{ 0 };

  void Handle(TargetEvent ev)
  {
    ++target_evt_handled;
  }
};

class IdConsumerTargetEvt
{
public:
  IdConsumerTargetEvt(size_t id) : id_{ id } {}
  IdConsumerTargetEvt(const IdConsumerTargetEvt&) = delete;
  IdConsumerTargetEvt(IdConsumerTargetEvt&&) = delete;

  const size_t id_;
  int target_evt_handled{ 0 };
  int blocker_received{ 0 };

  void Handle(TargetEvent ev)
  {
    ++target_evt_handled;
  }

  void Handle(BlockerEvent ev)
  {
    ++blocker_received;
    std::this_thread::sleep_for(500ms);
  }
};

class IdConsumerNoTargetEvt
{
public:
  IdConsumerNoTargetEvt(size_t id) : id_{ id } {}
  IdConsumerNoTargetEvt(const IdConsumerNoTargetEvt&) = delete;
  IdConsumerNoTargetEvt(IdConsumerNoTargetEvt&&) = delete;

  const size_t id_;
  int no_target_evt_handled{ 0 };

  void Handle(NoTargetEvent ev)
  {
    ++no_target_evt_handled;
  }
};

class ConsumerAgent
{
public:
  ConsumerAgent() = default;
  ConsumerAgent(const ConsumerAgent&) = delete;
  ConsumerAgent(ConsumerAgent&&) = delete;

  int event_handled{ 0 };

  void Handle(NoTargetEvent ev)
  {
    ++event_handled;
  }
};

// TEST DISPATCHER

using WorkerType = WorkerUnitMutex;

class GlobalDispatcher : public GlobalDispatcherBase<WorkerType>
{
public:

  GlobalDispatcher(EventCatbus<WorkerType>& global_bus) : GlobalDispatcherBase<WorkerType>{ global_bus } {};

  template<typename Event>
  void Route(Event event) noexcept(false)
  {
    GlobalDispatcherBase<WorkerType>::Route(std::move(event), *a, *b);
  }

  void SetDomain(NoIdConsumerNoTargetEvt* p)  { a = p; }
  void SetDomain(IdConsumerTargetEvt* p) { b = p; }

private:
  NoIdConsumerNoTargetEvt* a{ nullptr };
  IdConsumerTargetEvt* b{ nullptr };
};

// TEST FUNCTIONS

/// Used for events without "size_t target" field.
bool BasicStaticDispatch()
{
  EventCatbus<WorkerType> catbus{ 1 };
  NoIdConsumerNoTargetEvt A;
  NoIdConsumerTargetEvt B;

  bool ok = !has_id<NoIdConsumerNoTargetEvt>::value && !has_id<NoIdConsumerTargetEvt>::value;
  if (!ok)
  {
    return false;
  }
  ok = has_handler<NoIdConsumerNoTargetEvt, NoTargetEvent>::value && !has_handler<NoIdConsumerTargetEvt, NoTargetEvent>::value;
  if (!ok)
  {
    return false;
  }
  static_dispatch(catbus, NoTargetEvent{}, B, A);
  std::this_thread::sleep_for(100ms);
  return ok = A.no_target_evt_handled == 1 && B.target_evt_handled == 0;
}

/// Used for events with "size_t target" field.
bool BasicDynamicDispatch()
{
  EventCatbus<WorkerType> catbus{ 1 };
  IdConsumerTargetEvt A{ 1 };
  IdConsumerNoTargetEvt B{ 2 };

  bool ok = has_id<IdConsumerTargetEvt>::value && has_id<IdConsumerNoTargetEvt>::value;
  if (!ok)
  {
    return false;
  }
  ok = has_handler<IdConsumerTargetEvt, TargetEvent>::value && !has_handler<IdConsumerNoTargetEvt, TargetEvent>::value;
  if (!ok)
  {
    return false;
  }
  dynamic_dispatch(catbus, TargetEvent{ 1 }, A, B);
  std::this_thread::sleep_for(100ms);
  return ok =  A.target_evt_handled == 1 && B.no_target_evt_handled == 0;
}

/// If candidate with proper id does not have handler for the event, exception should be thrown.
bool FailedDynDispatchNoHandler()
{
  EventCatbus<WorkerType> catbus{ 1 };
  IdConsumerTargetEvt A{ 1 };
  IdConsumerNoTargetEvt B{ 2 };

  bool exception_caught{};
  try
  {
    dynamic_dispatch(catbus, TargetEvent{ 2 }, A, B);
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
  EventCatbus<WorkerType> catbus{ 1 };
  IdConsumerTargetEvt A{ 2 };
  IdConsumerTargetEvt B{ 1 };

  bool exception_caught{};
  try
  {
    dynamic_dispatch(catbus, TargetEvent{ 3 }, A, B);
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
  EventCatbus<WorkerType> catbus{ 2 };
  GlobalDispatcher dispatcher{ catbus };
  NoIdConsumerNoTargetEvt A;
  dispatcher.SetDomain(&A);
  dispatcher.Route(NoTargetBlockerEvent{});
  dispatcher.Route(NoTargetEvent{});

  std::this_thread::sleep_for(100ms);

  bool ok = A.blocker_received == 1 && A.no_target_evt_handled == 1;
  return ok;
}

/// Used for consumers, who has "const size_t id_" member.
bool OrderedScheduling()
{
  EventCatbus<WorkerType> catbus{ 2 };
  GlobalDispatcher dispatcher{ catbus };
  IdConsumerTargetEvt A{ 1 };
  dispatcher.SetDomain(&A);
  dispatcher.Route(BlockerEvent{ 1 });
  dispatcher.Route(TargetEvent{ 1 });

  std::this_thread::sleep_for(100ms);

  bool ok = A.blocker_received == 1 && A.target_evt_handled == 0;
  return ok;
}

/// Send task directly to event bus, using helper function 'make_handle_task'
/// with custom factory function.
bool NoRoutingTask()
{
  EventCatbus<WorkerType> catbus{ 1 };
  ConsumerAgent agent;
  catbus.Send(
    make_handle_task(
      NoTargetEvent{},
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

