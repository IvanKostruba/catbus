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
