#pragma once

#include "dispatch_utils.h"
#include "event_bus.h"

namespace catbus {

/// Helper object to manage event routing between different domains unaware of each other.

/// Functions from dispatch_utils are used to select object, capable to process
/// the event.
/// This class is needed to hide domain/consumers types and instances from each other.
/// It's not possible to store some callable object, incapsulating this information,
/// because events do not have specific type, and it's impossible to have
/// std::function < void( auto) >
class GlobalDispatcherBase
{
public:
  GlobalDispatcherBase(EventCatbus& global_bus) : global_bus_{ global_bus } {};

  template<typename Event, typename ...Ts>
  typename std::enable_if_t<has_target<Event>::value> Route(Event event, Ts& ...consumers) noexcept(false)
  {
    dynamic_dispatch(global_bus_, std::move(event), consumers...);
  }

  template<typename Event, typename ...Ts>
  typename std::enable_if_t<!has_target<Event>::value> Route(Event event, Ts& ...consumers)
  {
    static_dispatch(global_bus_, std::move(event), consumers...);
  }

private:
  EventCatbus & global_bus_;
};

}; // namespace catbus
