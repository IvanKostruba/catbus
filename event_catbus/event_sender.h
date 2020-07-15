#pragma once

#include "dispatch_utils.h"
#include "event_bus.h"

#include <variant>

namespace catbus {
  namespace detail {

    template<typename Bus, typename Event, typename... Ts>
    void route(Bus& bus, Event event, Ts& ...consumers) noexcept(false)
    {
      if constexpr (has_target<Event>::value)
      {
        dynamic_dispatch(bus, std::move(event), consumers...);
      }
      else
      {
        static_dispatch(bus, std::move(event), consumers...);
      }
    }

    struct EmptyEventsList {};

  }; // namespace detail

  // Inherit from this class if you need to send events through dispatcher.
  template <typename... E>
  struct EventSender
  {
    using event_type = std::conditional_t<(sizeof...(E) > 0), std::variant<E...>, detail::EmptyEventsList>;
    std::function<void(event_type)> Send;
  };

  template <typename Bus, typename... Consumer>
  void setup_dispatch(Bus& bus, Consumer&... consumers)
  {
    auto discard =
    {
      (consumers.Send = [&](typename Consumer::event_type&& ev)
      {
        if constexpr (!std::is_same_v<typename Consumer::event_type, detail::EmptyEventsList>)
        {
          std::visit(
            [&](auto&& event) { detail::route(bus, std::move(event), consumers...); },
            ev
          );
        }
      }, 0) ...
    };
  }

}; // namespace catbus
