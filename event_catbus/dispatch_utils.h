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

#include "event_bus.h"

#include <tuple>
#include <type_traits>
#include <utility>
#include <exception>

/// This header contains utility methods for distributing events between producers/consumers, who are unaware of each other.
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
typename std::enable_if<!has_handler<Consumer, Event>::value || !has_id<Consumer>::value, bool>::type
route_event(EventCatbus& bus, Event& ev, Consumer& c)
{
  return false;
}

/// This function will instantiate for classes, that have handler given event.
template <typename Event, class Consumer>
typename std::enable_if<has_handler<Consumer, Event>::value && has_id<Consumer>::value, bool>::type
route_event(EventCatbus& bus, Event& ev, Consumer& c)
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
typename std::enable_if<!has_id<Consumer>::value, void>::type
static_route(EventCatbus& bus, Event& ev, Consumer& c)
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
typename std::enable_if<has_id<Consumer>::value, void>::type
static_route(EventCatbus& bus, Event& ev, Consumer& c)
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
