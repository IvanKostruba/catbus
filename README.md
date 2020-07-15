# catbus
Lightweight framework for event-driven systems

This is a header-only library, it resides fully in event_catbus directory. VS solution is just a test application for the library.

## event_bus.h
Contains `EventCatbus` class which implements a pool of worker threads where event handling will run. Based on 'Active Objects' pattern.

## dispatch_utils.h
Provide some helper functions and types, mainly `static_dispatch()` and `dynamic_dispatch()` that can be used directly to route events between consumers.

## event_sender.h
Contains class EventSender which you can inherit from if you want to set up an automatic dispatch of events, and `setup_dispatch()` function, that takes a pack of EventSender descendants and initializes their `Send()` methods so that they can use it to dispatch events automatically.

## Usage overview:
'Event' is just any type, if it is move-constructible and move-assignable, then no copies will be created in dispatch process.
Event consumer must have `Handle()` method(s), taking 'Event' type as an argument by value. Dispatcher will automatically find proper consumer based on Handle methods signatures.

In basic scenario just type of event and signature of Handle() method determines, how event is dispatched, and the first object in `static_dispatch()` arguments list that has suitable `Handle()` method will be selected.

In case you need to dispatch between several instances of the same type, you can put `const size_t id_` field in the handlers and `size_t target` field in events, and then use `dynamic_dispatch()`, which will select between handlers with proper methods the one with `id_ == target`.

If you want to (almost) guarantee that events will come to consumer in the same order they were sent, you can add `const size_t affinity_` field to the handler. Then events handling will always be sceduled to the same worker unit.

When you do not want event processing modules to know about each other, or just want some more convenience, you can inherit your modules from `EventSender` struct, and then call `setup_dispatch()` on them and the bus. After it, your modules will have `Send()` method that will take care of event routing, automatically choosing (at compile time) between static and dynamic dispatch.

Please see 'example.cpp' for quick reference and 'CatbusLib.cpp' for more comprehensive examples.
