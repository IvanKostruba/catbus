# catbus
Lightweight framework for event-driven systems

This is a header-only library, it resides fully in event_catbus directory. VS solution is just a test application for the library.

## event_bus.h
Contains EventCatbus class which implements a pool of worker threads where event handling will run. Based on 'Active Objects' pattern.

## dispatch_utils.h
Provide some helper functions and types, mainly static_dispatch() and dynamic_dispatch() that can be used directly to route events between consumers.

## event_sender.h
Contains class EventSender which you can inherit from if you want to set up an automatic dispatch of events, and setup_dispatch() function, that takes a pack of EventSender descendants and initializes their Send() methods so that they can use it to dispatch events without need to know about each other.

## Usage overview:
'Event' is just any type, if it is move-constructible and move-assignable, then no copies will be created in dispatch process.
Event consumer must have Handle() method(s), taking 'Event' type as an argument by value. Dispatcher will automatically find proper consumer based on Handle methods signatures.

Please see 'example.cpp' for quick reference and 'CatbusLib.cpp' for more comprehensive examples.
