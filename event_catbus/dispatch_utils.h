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
#include "exception.h"

#include <tuple>
#include <type_traits>
#include <utility>

// This header contains utility methods for dispatching events to proper consumers based on the
// handlers they are providing and potentially also target vs. id_ comparison.
namespace catbus {

//--------------------- SFINAE event handler detector

// Check if class T has method 'T::Handle(Event evt)' to process event of specific type.

template<class...>
using void_t = void;

template<class T, class Event, class = void>
struct has_handler : std::false_type {};

template<class T, class Event>
struct has_handler<T, Event, void_t<decltype(std::declval<T>().Handle(std::declval<Event>()))>> : std::true_type {};

//--------------------- SFINAE event target id detector

// Check if type Event has member 'size_t target'.

template<class Event, class = void>
struct has_target : std::false_type {};

template<class Event>
struct has_target<Event, void_t<std::enable_if_t<std::is_same_v<decltype(Event::target), size_t>>>> : std::true_type {};

//--------------------- SFINAE consumer id detector

// Check if type Consumer has member 'const size_t id_'.

template<class Consumer, class = void>
struct has_id : std::false_type {};

template<class Consumer>
struct has_id<Consumer, void_t<std::enable_if_t<std::is_same_v<decltype(Consumer::id_), const size_t>>>> : std::true_type {};

//--------------------- SFINAE handler caller for specific target

// This function will instantiate for classes, that have handler given event.
template <typename Catbus, typename Event, class Consumer>
bool route_event(Catbus& bus, Event& ev, Consumer& c)
{
  if constexpr (has_handler<Consumer, Event>::value && has_id<Consumer>::value)
  {
    if (c.id_ != ev.target)
    {
      return false;
    }
    auto l = [&consumer = c, event{ std::move(ev) }] () mutable -> void
      {
        consumer.Handle(std::move(event));
      };
    bus.Send(std::move(l));
    return true;
  }
  return false;
}

//--------------------- Dynamic runtime dispatcher

template <typename Catbus, typename Event, class Consumer>
void dynamic_dispatch(Catbus& bus, Event ev, Consumer& c) noexcept(false)
{
  static_assert(has_target<Event>::value, "Event does not have 'size_t target' member.");
  if (!route_event(bus, ev, c))
  {
    throw dispatch_error{ev.target};
  }
}

// Recursively search parameter pack for types with handlers for given event, call handler for one, that has corresponding id.
template <typename Catbus, typename Event, class Consumer, class ...Consumers>
void dynamic_dispatch(Catbus& bus, Event ev, Consumer& c, Consumers&... others) noexcept(false)
{
  static_assert(has_target<Event>::value, "Event does not have 'size_t target' member.");
  if (!route_event(bus, ev, c))
  {
    dynamic_dispatch(bus, std::move(ev), others...);
  }
}

//--------------------- Static dispatch helper

// This function is needed to break recursion in compile-time, but it will be selected only
// when no handlers are found in parameter pack, so compilation will break. There is static assert
// using this return value to generate conscious error message.
template<typename Event>
constexpr size_t find_handler_idx(size_t idx)
{
  return idx;
}

// Recursively search for type with handler for given event.
template<typename Event, typename Head, typename ...Ts>
constexpr size_t find_handler_idx(size_t idx = 0)
{
  return has_handler< Head, Event >::value ? idx : find_handler_idx<Event, Ts...>(idx + 1);
}

//--------------------- Static compile-time dispatcher

// Consumer does not have id, so event processing is scheduled to any thread
template <typename Catbus, typename Event, class Consumer>
void static_route(Catbus& bus, Event& ev, Consumer& c)
{
  auto l = [&consumer = c, event{ std::move(ev) }] () mutable -> void
    {
      consumer.Handle(std::move(event));
    };
  bus.Send(std::move(l));
}

// Call handler for first consumer in parameter pack, that capable of handling the event.
template<typename Catbus, typename Event, class ...Consumers>
void static_dispatch(Catbus& bus, Event ev, Consumers& ...args)
{
  static_assert(std::tuple_size<std::tuple<Consumers...>>::value > find_handler_idx<Event, Consumers...>(), "Handler not found!");
  std::tuple<Consumers&...> list{ args... };
  static_route(bus, ev, std::get<find_handler_idx<Event, Consumers...>()>(list));
}

}; // namespace catbus
