# catbus
Framework for event-driven systems

This is a header-only library, it resides fully in event_catbus directory. VS solution is just a test application for the library.

Short usage overview:

Any type can be an event, and all that's needed to process it is another class with void Handle() method,
accepting that type. Dispatcher will automatically find proper consumer and pass the event to it.

Example:

// Event

struct MyEvent
{
  std::string data{"Hello world!"};
};

// Consumer

class MyDomain
{
public:
  void Handle(MyEvent event) { std::cout << event.data << "\n"; }
};

// Let the global dispatcher know about your new consumer

using WorkerType = WorkerUnitMutex;
class GlobalDispatcher : public GlobalDispatcherBase<WorkerType>
{
public:
  ...
  template<typename Event>
  void Route(Event event) noexcept(false)
  {
    GlobalDispatcherBase<WorkerType>::Route(std::move(event), *domainInstancePointer);
  }

private:
  MyDomain* domainInstancePointer; // should be set in runtime
};

// Now from anywhere:

globalDispatcher.Route( MyEvent{} );

// or

static_dispatch(eventBus, MyEvent{}, DomainA_instance, DomainB_instance, MyDomain_instance);
dynamic_dispatch(eventBus, MyEventWithTarget{}, DomainA_instance, DomainB_instance, MyDomain_instance);

And MyDomain instance will have it! That's it.