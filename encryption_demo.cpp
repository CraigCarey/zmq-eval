// A super simple application to demonstrate authentication and Curve security with plain ZeroMQ
// Find more info on http://www.evilpaul.org/wp/2017/05/02/authentication-encryption-zeromq/

#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <zmq.h>

// Configuration options to tweak..
#define SERVER_BIND_ENDPOINT "tcp://*:9000"
#define SERVER_CONNECT_ENDPOINT "tcp://127.0.0.1:9000"
#define NUM_CLIENTS 1
#define NUM_BAD_CLIENTS 0

// Uncommenting this causes a ZAP server to be set up to handle further authentication
//#define DO_EXTRA_AUTHENTICATION

// Utility function to send a string over a socket
// Set send_more to true if there are more strings to come in this message
void SendString(void* socket, const std::string& message, const bool send_more)
{
    zmq_send(socket, message.c_str(), message.length(), send_more ? ZMQ_SNDMORE : 0);
}

// Utility function to receive a string over a socket
// A timeout value of -1 means no timeout (ie: block-wait)
// Returns an empty string if nothing was received
std::string ReceiveString(void* socket, const int timeout_in_ms = -1)
{
    zmq_pollitem_t poll_items[] = {socket, 0, ZMQ_POLLIN, 0};
    const int rc = zmq_poll(poll_items, 1, timeout_in_ms);
    if (poll_items[0].revents && ZMQ_POLLIN)
    {
        char buf[1024];
        const int rc = zmq_recv(socket, buf, 1024, 0);
        return std::string(buf, rc);
    }
    else
    {
        return "";
    }
}

// Utility function to output to the console. Use a mutex so that we don't get interleaved output
void DebugOut(const std::string& source, const std::string& message)
{
    static std::mutex mutex;
    std::lock_guard lock_guard(mutex);
    const std::thread::id id = std::this_thread::get_id();
    std::cout << "[" << source << ":" << id << "] " << message << "\n";
}

// Server thread. Open up a PUB socket, send a bunch of messages and then exit
void ServerThread(void* ctx, const char* server_secret_key)
{
    // Show keys
    DebugOut("Server", "Server secret key: " + std::string(server_secret_key));

    //  Create socket and set options
    void* server = zmq_socket(ctx, ZMQ_PUB);

    // Mark us as a server and set out secret key. You have to do this before binding or else it
    // won't work
    const int curve_server_enable = 1;
    zmq_setsockopt(server, ZMQ_CURVE_SERVER, &curve_server_enable, sizeof(curve_server_enable));
    zmq_setsockopt(server, ZMQ_CURVE_SECRETKEY, server_secret_key, 40);

    // Bind the socket ready for the client to connect
    DebugOut("Server", "Binding..");
    zmq_bind(server, SERVER_BIND_ENDPOINT);

    // Send some messages
    for (auto i = 0; i < 10; i++)
    {
        const std::string message = "Hello " + std::to_string(i + 1);
        DebugOut("Server", "Sending message: " + message);
        SendString(server, message, false);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    // Tidy up
    DebugOut("Server", "Finished");
    zmq_close(server);
}

// Client thread. Open up a SUB socket, subscribe to the server and then read and print messages
void ClientThread(const char* server_public_key)
{
    // Generate client keys
    char client_public_key[41], client_secret_key[41];
    zmq_curve_keypair(client_public_key, client_secret_key);

    // Show keys
    DebugOut("Client", "Client public key: " + std::string(client_public_key));
    DebugOut("Client", "Client secret key: " + std::string(client_secret_key));
    DebugOut("Client", "Server public key: " + std::string(server_public_key));

    // Our own context
    void* ctx = zmq_ctx_new();

    //  Create SUB socket and subscribe to everything
    void* client = zmq_socket(ctx, ZMQ_SUB);
    zmq_setsockopt(client, ZMQ_SUBSCRIBE, "", 0);

    // Set the server's public key, and our public and secret keys. You have to do this before
    // binding or else it won't work
    zmq_setsockopt(client, ZMQ_CURVE_SERVERKEY, server_public_key, 40);
    zmq_setsockopt(client, ZMQ_CURVE_PUBLICKEY, client_public_key, 40);
    zmq_setsockopt(client, ZMQ_CURVE_SECRETKEY, client_secret_key, 40);

    // Connect the socket to the server
    DebugOut("Client", "Connecting..");
    zmq_connect(client, SERVER_CONNECT_ENDPOINT);

    // Receive messages and print them. Exit when we haven't received a message for a while. If
    // the connection was not allowed then we won't receive any messages at all
    while (true)
    {
        const std::string received_message = ReceiveString(client, 2000);
        if (received_message.empty())
        {
            DebugOut("Client", "Didn't receive anything for a while. Exiting..");
            break;
        }
        DebugOut("Client", "Received: " + received_message);
    }

    // Tidy up
    DebugOut("Client", "Finished");
    zmq_close(client);
    zmq_ctx_term(ctx);
}

// ZAP authentication thread to receive and process authentication rtequests. Note that
// authentication is an entirely optional step on top of the standard CURVE authentication method
// of ensuring that the client has the correct public key for the server that it is connecting to
// NB: If you do set up extra authentication then you MUST ensure that the ZAP socket is bound
// before you open up any other sockets. If you don't then it is possible that a connection could
// be authenticated using the default ZeroMQ authentication method (ie: none) before your method
// has been properly registered. We check this here using a cheap-and-cheerful flag which blocks
// the main thread from starting the server until the socket is bound. You must also ensure that
// the socket stays bound while you are still accepcting connections. If the socket is un-bound
// then ZeroMQ will go back to the default behaviour of accepting all connection requests that
// present the correct public key
#if defined DO_EXTRA_AUTHENTICATION
void AuthThread(void* ctx, bool& authentication_ready_flag)
{
    // Bind a socket to the ZAP inproc channel
    void* sock = zmq_socket(ctx, ZMQ_REP);
    zmq_bind(sock, "inproc://zeromq.zap.01");
    authentication_ready_flag = true;

    // Receive requests and print them. Exit when we haven't received a request for a while. In
    // the real world we would keep reading requests until told to exit
    while (true)
    {
        // Wait for a request to come in and then read it
        const std::string version = ReceiveString(sock, 2000);
        if (version.empty())
        {
            DebugOut("Auth", "Didn't receive anything for a while. Exiting..");
            break;
        }
        const std::string request_id = ReceiveString(sock);
        const std::string domain = ReceiveString(sock);
        const std::string address = ReceiveString(sock);
        const std::string identity_property = ReceiveString(sock);
        const std::string mechanism = ReceiveString(sock);
        const std::string client_key = ReceiveString(sock);

        // Show the request details
        char client_key_text[41];
        zmq_z85_encode(client_key_text, (uint8_t*)client_key.c_str(), 32);
        DebugOut("Auth",
                 "Received ZAP request:"
                 "\n\tversion: " +
                     version + "\n\trequest_id: " + request_id + "\n\tdomain: " + domain + "\n\taddress: " + address +
                     "\n\tidentity_property: " + identity_property + "\n\tmechanism: " + mechanism +
                     "\n\tclient_key: " + client_key_text);

        // Accept or deny this connection request. Change this to false and see what happens. In a
        // real application we would do this based on the information that came with the ZAP request -
        // ie: ip address and client's public key. If we wanted to authenticate on the client's public
        // key then we would need to distribute it to the server somehow
        const bool accept_this_connection = true;
        DebugOut("Auth",
                 (accept_this_connection ? "Accepting this connection request" : "Denying this connection request"));

        // Send reply back to ZAP. A reply of 200 means accept, 400 means deny
        SendString(sock, "1.0", true);
        SendString(sock, request_id, true);
        SendString(sock, accept_this_connection ? "200" : "400", true);
        SendString(sock, "", true);
        SendString(sock, "", true);
        SendString(sock, "", false);
    }

    // We're only serving one ZAP request in this example, so we'll close the socket and end the
    // thread once we've done that
    DebugOut("Auth", "Finished");
    zmq_close(sock);
}
#endif

// Main application. Start up client, server and (optionally) authentication threads and wait for them to exit
int main()
{
    // Check that we built ZeroMQ with Curve support
    if (!zmq_has("curve"))
    {
        DebugOut("Main", "ZeroMQ library has not been built with Curve support");
        return 0;
    }

    // Server and auth threads need to talk over inproc so they need to share a context. This is
    // ok because they will always run within the same process. The client thread would be running
    // on another machine/process so would not share the same context
    void* ctx = zmq_ctx_new();

    // Generate server keys. The server thread needs to know the private key and the client thread
    // needs to know the public key. In a real application we would need to distribute the
    // server's public key to the client somehow but we'll cheat a bit here
    char server_public_key[41], server_secret_key[41];
    zmq_curve_keypair(server_public_key, server_secret_key);
    DebugOut("Main", "Server public key: " + std::string(server_public_key));
    DebugOut("Main", "Server secret key: " + std::string(server_secret_key));

    // We'll start a bunch of threads..
    std::vector<std::thread> threads;

#if defined DO_EXTRA_AUTHENTICATION
    // Start the authentication thread if we want it. Wait until the socket is bound before
    // opening other sockets
    bool authentication_ready_flag = false;
    threads.emplace_back(std::thread(AuthThread, ctx, std::ref(authentication_ready_flag)));
    while (!authentication_ready_flag)
    {
    }
#endif

    // Start the server thread
    threads.emplace_back(std::thread(ServerThread, ctx, server_secret_key));

    // Start some clients
    for (auto i = 0; i < NUM_CLIENTS; i++)
    {
        threads.emplace_back(std::thread(ClientThread, server_public_key));
    }

    // Start some clients that have the wrong public key for the server to show that they will not
    // connect or receive messages
    for (auto i = 0; i < NUM_BAD_CLIENTS; i++)
    {
        threads.emplace_back(std::thread(ClientThread, "ThisIsTheWrongPublicKeyForTheServer!!!!!"));
    }

    // Wait for the threads to finish
    for (auto& thread : threads)
    {
        thread.join();
    }

    // Tidy up
    DebugOut("Main", "Finished");
    zmq_ctx_term(ctx);
}
