#include <fstream>
#include <iostream>
#include <streambuf>
#include <string>

#include <zmq.hpp>

int main()
{
    // Read in a sample json file
    std::ifstream json_fs("../3mb.json");
    std::stringstream json_ss;
    json_ss << json_fs.rdbuf();

    // initialize the zmq context with a single IO thread
    zmq::context_t context{1};

    // construct a REQ (request) socket and connect to interface
    zmq::socket_t socket{context, zmq::socket_type::req};
    socket.connect("tcp://localhost:5555");

    for (auto request_num = 0; request_num < 10; ++request_num)
    {
        // send the request message
        std::cout << "Sending JSON " << request_num << "...\n";
        socket.send(zmq::buffer(json_ss.str()), zmq::send_flags::none);

        // wait for reply from server
        zmq::message_t reply{};
        socket.recv(reply, zmq::recv_flags::none);

        std::cout << "Received " << reply.to_string();
        std::cout << " (" << request_num << ")\n";
    }
}
