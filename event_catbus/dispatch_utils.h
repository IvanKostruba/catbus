#pragma once

#include "event_bus.h"

#include <tuple>
#include <type_traits>
#include <utility>
#include <exception>

/// This header contains utility methods for distributing events between producers/consumers, who are unaware of each other.

/// Any type can be an event, and all that's needed to process it is another class with void Handle() method,
/// accepting that type. Dispatcher will automatically find proper consumer and pass the event to it.
///
/// Example:
///
///  // Event
///  struct MyEvent
///  {
///    std::string data{"Hello world!"};
///  };
///
///  // Consumer
///  class MyDomain
///  {
///  public:
///    void Handle(MyEvent event) { std::cout << event.data << "\n"; }
///  };
///
///  // Let the global dispatcher know about your new consumer
///  class GlobalDispatcher : public GlobalDispatcherBase
///  {
///  public:
///    ...
///    template<typename Event>
///    void Route(Event event) noexcept(false)
///    {
///      GlobalDispatcherBase::Route(std::move(event), *domainInstancePointer);
///    }
///  
///  private:
///    MyDomain* domainInstancePointer; // should be set in runtime
///  };
///
///  // Now from anywhere:
///  static_dispatch(eventBus, MyEvent{}, DomainA_instance, DomainB_instance, MyDomain_instance);
///  dynamic_dispatch(eventBus, MyEventWithTarget{}, DomainA_instance, DomainB_instance, MyDomain_instance);
///
///  // or
///
///  globalDispatcher.Route( MyEvent{} );
///
///  And MyDomain instance will have it! That's it.

namespace catbus {

//--------------------- SFINAE event handler detector

/// Check if class T has method 'T::Handle(Event evt)' to process event of type Event.

template<class...>
using void_t = void;

template<class T, class Event, class = void>
struct has_handler : std::false_type {};

template<class T, class Event>
struct has_handler<T, Event, void_t<decltype(std::declval<T>().Handle(std::declval<Event>()), (void)0)>> : std::true_type {};

//--------------------- SFINAE event target id detector

/// Check if type Event has member 'size_t target'.

template<class Event, class = void>
struct has_target : std::false_type {};

template<class Event>
struct has_target<Event, void_t<typename std::enable_if<std::is_same<decltype(Event::target), size_t>::value>::type>> : std::true_type {};

//--------------------- SFINAE consumer id detector

/// Check if type Consumer has member 'const size_t id_'.

template<class Consumer, class = void>
struct has_id : std::false_type {};

template<class Consumer>
struct has_id<Consumer, void_t<typename std::enable_if<std::is_same<decltype(Consumer::id_), const size_t>::value>::type>> : std::true_type {};

//--------------------- SFINAE handler caller for specific target

/// This function will instantiate for classes, that do not have handler for event of type 'Event'.
template <typename Event, class Consumer>
bool route_event(EventCatbus& bus, Event& ev, Consumer& c,
  typename std::enable_if<!has_handler<Consumer, Event>::value || !has_id<Consumer>::value, int>::type = 0)
{
  return false;
}

/// This function will instantiate for classes, that have handler given event.
template <typename Event, class Consumer>
bool route_event(EventCatbus& bus, Event& ev, Consumer& c,
  typename std::enable_if<has_handler<Consumer, Event>::value && has_id<Consumer>::value, int>::type = 0)
{
  if (c.id_ != ev.target)
  {
    return false;
  }
  bus.Send(
    c.id_,
    [ &consumer = c,
      event{ std::move(ev) } ]
    () mutable -> void
    {
      consumer.Handle( std::move( event ) );
    } );
  return true;
}

//--------------------- Dynamic dispatcher exception

class dispatch_error : public std::exception
{
public:
  const size_t target_id_;

  dispatch_error(size_t target_id) : target_id_{target_id}
  {
  }

  const char* what() const noexcept override
  {
    return description;
  }

private:
  const char* description = "No consumers with corresponding id were found";
};

//--------------------- Dynamic runtime dispatcher

template <typename Event, class Consumer>
void dynamic_dispatch(EventCatbus& bus, Event ev, Consumer& c) noexcept(false)
{
  static_assert(has_target<Event>::value, "Event does not have 'size_t target' member.");
  if (!route_event(bus, ev, c))
  {
    throw dispatch_error{ev.target};
  }
}

/// Recursively search parameter pack for types with handlers for given event, call handler for one, that has corresponding id.
template <typename Event, class Consumer, class ...Consumers>
void dynamic_dispatch(EventCatbus& bus, Event ev, Consumer& c, Consumers&... others) noexcept(false)
{
  static_assert(has_target<Event>::value, "Event does not have 'size_t target' member.");
  if (!route_event(bus, ev, c))
  {
    dynamic_dispatch(bus, std::move(ev), others...);
  }
}

//--------------------- Static dispatch helper

template<typename Event>
constexpr size_t find_handler_idx(size_t idx)
{
  return idx;
}

/// Recursively search for type with handler for given event.
template<typename Event, typename Head, typename ...Ts>
constexpr size_t find_handler_idx(size_t idx = 0)
{
  return has_handler< Head, Event >::value ? idx : find_handler_idx<Event, Ts...>(idx + 1);
}

//--------------------- Static compile-time dispatcher

/// Consumer does not have id, so event processing is scheduled to any thread
template <typename Event, class Consumer>
void static_route(EventCatbus& bus, Event& ev, Consumer& c,
  typename std::enable_if<!has_id<Consumer>::value, int>::type = 0)
{
  bus.Send(
    [&consumer = c,
    event { std::move(ev) }]
  () mutable -> void
  {
    consumer.Handle(std::move(event));
  });
}

/// Consumer has id, so event processing is scheduled to specific thread
template <typename Event, class Consumer>
void static_route(EventCatbus& bus, Event& ev, Consumer& c,
  typename std::enable_if<has_id<Consumer>::value, int>::type = 0)
{
  bus.Send(
    c.id_,
    [&consumer = c,
    event { std::move(ev) }]
  () mutable -> void
  {
    consumer.Handle(std::move(event));
  });
}

/// Call handler for first consumer in parameter pack, that capable of handling the event.
template<typename Event, class ...Consumers>
void static_dispatch(EventCatbus& bus, Event ev, Consumers& ...args)
{
  static_assert(std::tuple_size<std::tuple<Consumers...>>::value > find_handler_idx<Event, Consumers...>(), "Handler not found!");
  std::tuple<Consumers&...> list{ args... };
  static_route(bus, ev, std::get<find_handler_idx<Event, Consumers...>()>(list));
}

/// Helper function to get unique object id
size_t get_unique_id()
{
  static size_t current_id;
  return current_id++;
}

/// Helper function to create task which passes event to handler, produced by factory function
template<typename Event, typename FactoryFunc>
auto make_handle_task(Event ev, FactoryFunc f)
{
  return
    [
      evt{ std::move(ev) },
      factory_func = f
    ]() mutable -> void
    {
      auto target_consumer = factory_func();
      if (target_consumer)
      {
        target_consumer->Handle(std::move(evt));
      }
    };
}

}; // namespace catbus
