// Example:

#include "dispatch_utils.h"
#include "event_bus.h"
#include "event_sender.h"
#include "worker_mutex.h"

// Events
struct Init
{};

struct Request
{
  const size_t sender;
  std::string data{"Hello world!"};
};

struct Response
{
  const size_t target;
  int error_code;
};

// Event handlers
class Sender : public catbus::EventSender<Request>
{
public:
  explicit Sender(size_t id) : id_{id}
  {}
  void Handle(Init event) { Send(Request{id_}); }
  
  const size_t id_;
};

class Receiver : public catbus::EventSender<Response>
{
public:
  void Handle(Request req) { Send(Response{req.sender, 200}); }
};

// Initialization
catbus::EventBus<WorkerUnitMutex> bus{2}; // The threads where handlers will run
Sender sender{1};
Receiver receiver;
catbus::setup_dispatch(bus, sender, receiver); // Setting up Send() methods

// Startup
catbus::static_dispatch(bus, Init{}, sender); // Send the initial event
