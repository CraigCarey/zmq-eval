#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include <zmq.hpp>

int main()
{
    using namespace std::chrono_literals;

    // initialize the zmq context with a single IO thread
    zmq::context_t context{1};

    // construct a REP (reply) socket and bind to interface
    zmq::socket_t socket{context, zmq::socket_type::rep};
    socket.bind("tcp://*:5555");

    zmq::socket_t socket2{context, zmq::socket_type::rep};
    socket.connect("tcp://:5555");

    // prepare some static data for responses
    const std::string data{"JSON Received in C++ Server"};

    for (;;)
    {
        zmq::message_t request;

        // receive a request from client
        socket.recv(request, zmq::recv_flags::none);

        std::cout << "Received " << request.to_string() << '\n';

        // simulate work
        std::this_thread::sleep_for(1s);

        // send the reply to the client
        socket.send(zmq::buffer(data), zmq::send_flags::none);
    }
}
