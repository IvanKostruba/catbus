#pragma once

#include "dispatch_utils.h"
#include "event_bus.h"

#include <type_traits>
#include <variant>

namespace catbus {
    namespace _detail {

    template<typename Bus, typename Event, typename ConsumersTuple, std::size_t... I>
    constexpr void route_impl(
        Bus& bus,
        Event event,
        ConsumersTuple& consumers,
        std::index_sequence<I...>
    ) noexcept(false) {
        if constexpr (has_target<Event>::value) {
            dynamic_dispatch(bus, std::move(event), *std::get<I>(consumers)...);
        } else {
            static_dispatch(bus, std::move(event), *std::get<I>(consumers)...);
        }
    }

    template<typename Bus, typename Event, typename ConsumersTuple>
    constexpr void route(Bus& bus, Event event, ConsumersTuple& consumers) noexcept(false) {
        route_impl(bus, std::move(event), consumers,
            std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<ConsumersTuple>>>{});
    }

    struct EmptyEventsList {};

    template<typename Event>
    struct sender_vtable {
        void (*send)(void* bus, void* consumers, Event event);

        void (*clone)(void* storage, const void* ptr);
    };

    template<typename Bus, typename ConsumersTuple, typename EventVar>
    constexpr sender_vtable<EventVar> sender_vtable_for {
        [](void* bus, void* consumers, EventVar ev) {
            if constexpr (!std::is_same_v<EventVar, _detail::EmptyEventsList>) {
                std::visit(
                    [&](auto&& event) { _detail::route(
                        *static_cast<Bus*>(bus),
                        std::move(event),
                        *static_cast<ConsumersTuple*>(consumers));
                    },
                    ev
                );
            }
        },

        [](void* storage, const void* ptr) {
            new (storage) ConsumersTuple{
                *static_cast<const ConsumersTuple*>(ptr)};
        }
    };

    }; // namespace _detail

// Use this class when you want to easily send given set of events to consumers.
// If you compose that into a consumer class with a name '_sender' it can be automatically
// set up by 'setup_dispatch' function.
template <typename... E>
struct EventSender {
    using event_type = std::conditional_t<(sizeof...(E) > 0), std::variant<E...>, _detail::EmptyEventsList>;

    EventSender() : _vtable{nullptr}, _bus{nullptr}
    {}

    template<typename Bus, typename... Consumer>
    EventSender(Bus& bus, Consumer&... consumers)
      : _vtable{&_detail::sender_vtable_for<Bus, std::tuple<Consumer*...>, event_type>}
    {
        static_assert(sizeof(std::tuple<Consumer*...>) <= sizeof(_consumers),
            "Wrapper buffer is too small!");
        _bus = &bus;
        new (&_consumers) std::tuple<Consumer*...>{&consumers...};
    }

    EventSender(const EventSender& other) {
        other._vtable->clone(&_consumers, &other._consumers);
        _bus = other._bus;
        _vtable = other._vtable;
    }

    EventSender& operator=(const EventSender& other) {
        other._vtable->clone(&_consumers, &other._consumers);
        _bus = other._bus;
        _vtable = other._vtable;
        return *this;
    }

    template<typename Bus, typename... Consumer>
    void init(Bus& bus, Consumer&... consumers)
    {
        static_assert(sizeof(std::tuple<Consumer*...>) <= sizeof(_consumers),
            "Wrapper buffer is too small!");
        _vtable = &_detail::sender_vtable_for<Bus, std::tuple<Consumer*...>, event_type>;
        _bus = &bus;
        new (&_consumers) std::tuple<Consumer*...>{&consumers...};
    }

    void send(event_type ev) {
        _vtable->send(_bus, &_consumers, std::move(ev));
    }

    const _detail::sender_vtable<event_type>* _vtable;
    void* _bus;
    std::aligned_storage_t<64> _consumers;
};

// This function will automatically init event senders with the name '_sender' inside the
// consumers instances passed here.
template <typename Bus, typename... Consumer>
void setup_dispatch(Bus& bus, Consumer&... consumers) {
    auto discard = {
        ([&](auto& consumer){
            if constexpr (has_sender<Consumer>::value) {consumer.sender_.init(bus, consumers...);}
        }(consumers), 0 ) ...
    };
    discard = {}; // suppress -Wunused-variable
}

}; // namespace catbus
