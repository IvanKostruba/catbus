# catbus
Framework for event-driven systems

This is a header-only library, it resides fully in event_catbus directory. VS solution is just a test application for the library.

Usage overview:

Any type can be an event, and all that's needed to process it is another class with void Handle() method,
accepting that type. Dispatcher will automatically find proper consumer and pass the event to it.
Please see 'example.cpp' for quick reference and 'CatbusLib.cpp' for more comprehensive samples.
