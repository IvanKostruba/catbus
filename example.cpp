// Example:

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

// And MyDomain instance will have it! That's it.