# catbus
Lightweight framework for event-driven systems

This is a header-only library, it resides fully in event_catbus directory. VS solution is just a test application for the library.

## event_bus.h
Contains `EventCatbus` class which incapsulates set of queues and a pool of worker threads where event handling will run.

## dispatch_utils.h
Provide some helper functions and types, mainly `static_dispatch()` and `dynamic_dispatch()` that can be used directly to route events between consumers.

## event_sender.h
Contains struct EventSender which you can compose into your class with the name `sender_` if you want to set up an automatic dispatch of events, and `setup_dispatch()` function, that takes a pack of instances and initializes their `sender_` members (if they have any) so that they can use it to dispatch events between each other.

## Usage overview:
'Event' is just any type, if it is move-constructible and move-assignable, then no copies will be created in dispatch process.
Event consumer must have `handle()` method(s), taking 'Event' type as an argument by value. Dispatcher will automatically find proper consumer based on Handle methods signatures.

In basic scenario just type of event and signature of `handle()` method determines, how event is dispatched, and the first object in `static_dispatch()` arguments list that has suitable handler will be selected.

In case you need to dispatch between several instances of the same type, you can put `const size_t id_` field in the handlers and `size_t target` field in events, and then use `dynamic_dispatch()`, which will select between handlers with proper handlers the one with `id_ == target`.

For convenience and module isolation, you can use `EventSender` struct for easy event dispatching, just include it in your class with name `sender_`, and then call `setup_dispatch()` on them and the bus. After it, your modules can call `sender_.send()` that will take care of event routing, automatically choosing (at compile time) between static and dynamic dispatch.

Please see 'example.cpp' for quick reference and 'CatbusLib.cpp' for more comprehensive examples. There's also 'performance.cpp' with some simple performance checks.
