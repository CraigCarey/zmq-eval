#include <fstream>
#include <iostream>
#include <streambuf>
#include <string>

#include <zmq.hpp>

class MessageSender final
{
public:
    MessageSender(const std::string& address, int port, int timeout_ms = 100)
        : context_(1), socket_(context_, zmq::socket_type::req)
    {
        // initialize the zmq context with a single IO thread
        zmq::context_t context{1};

        timeout_ms = 2000;  // Server has a 1s sleep

        // only wait timeout_ms for response
        socket_.set(zmq::sockopt::rcvtimeo, timeout_ms);

        // on context destruction don't wait for sends to complete
        socket_.set(zmq::sockopt::linger, 0);

        socket_.connect("tcp://" + address + ":" + std::to_string(port));
    }

    ~MessageSender() noexcept
    {
        context_.shutdown();
        socket_.close();
        context_.close();
    }

    std::optional<std::string> send_message(const std::string& message)
    {
        socket_.send(zmq::buffer(message), zmq::send_flags::none);

        // receive a reply from server - times out after timeout_ms
        zmq::message_t reply{};
        zmq::recv_result_t result = socket_.recv(reply, zmq::recv_flags::none);

        if (!result)
        {
            std::cerr << "Sending failed!\n";
            return std::nullopt;
        }

        return reply.to_string();
    }

private:
    zmq::context_t context_;
    zmq::socket_t socket_;
};

int main()
{
    // Read in a sample json file
    std::ifstream json_fs("../3mb.json");
    std::stringstream json_ss;
    json_ss << json_fs.rdbuf();

    MessageSender ms("localhost", 5555);

    auto response = ms.send_message(json_ss.str());

    if (response)
    {
        std::cout << "Received: \"" << *response << "\"\n";
    }
}
