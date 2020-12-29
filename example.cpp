// Example:

#include "dispatch_utils.h"
#include "event_bus.h"
#include "event_sender.h"
#include "queue_mutex.h"

#include <iostream>

using namespace std::chrono_literals;

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
  size_t target;
  int error_code;
};

// Event handlers
class Sender : public catbus::EventSender<Request>
{
public:
  explicit Sender(size_t id) : id_{id}
  {}
  void handle(Init event)
  { 
    std::cout << "Init received\n";
    send( Request{id_} );
  }
  void handle(Response event)
  { 
    std::cout << "Response received: code " << event.error_code << "\n";
  }
  
  const size_t id_;
};

class Receiver : public catbus::EventSender<Response>
{
public:
  void handle(Request req)
  { 
    std::cout << "Request received: " << req.data << "\n";
    send(Response{req.sender, 200});
  }
};

int main(int argc, char** argv) {
  // Initialization
  // Here the queues for events and threads for handlers execution will live.
  catbus::EventCatbus<catbus::MutexProtectedQueue, 1, 2> bus;
  Sender sender{1};
  Receiver receiver;
  catbus::setup_dispatch(bus, sender, receiver); // Setting up send() methods.

  // Startup
  catbus::static_dispatch(bus, Init{}, sender); // send the initial event.
  std::this_thread::sleep_for(200ms);
}

