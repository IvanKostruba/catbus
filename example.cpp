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
class Sender 
{
public:
  explicit Sender(size_t id) : id_{id}
  {}
  void handle(Init event)
  { 
    std::cout << "Init received\n";
    sender_.send( Request{id_} );
  }
  void handle(Response event)
  { 
    std::cout << "Response received: code " << event.error_code << "\n";
  }
  
  catbus::EventSender<Request> sender_;
  const size_t id_;
};

class Receiver
{
public:
  void handle(Request req)
  { 
    std::cout << "Request received: " << req.data << "\n";
    sender_.send(Response{req.sender, 200});
  }

  catbus::EventSender<Response> sender_;
};

int main(int argc, char** argv) {
  // Initialization
  // Here the queues for events and threads for handlers execution will live.
  catbus::EventCatbus<catbus::MutexProtectedQueue, 1, 2> bus;
  Sender sender{1};
  Receiver receiver;
  catbus::setup_dispatch(bus, sender, receiver); // Setting up sender_ members.

  // Startup
  catbus::static_dispatch(bus, Init{}, sender); // send the initial event.
  std::this_thread::sleep_for(200ms);
}
