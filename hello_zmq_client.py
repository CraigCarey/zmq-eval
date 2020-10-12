#
#   Hello World client in Python
#   Connects REQ socket to tcp://localhost:5555
#   Sends json string to server, expects "JSON Received" back
#

import zmq

context = zmq.Context()

#  Socket to talk to server
print("Connecting to hello world server…")
socket = context.socket(zmq.REQ)
socket.connect("tcp://localhost:5555")

json_file = open('3mb.json', 'r')
json_data = json_file.read()

#  Do 10 requests, waiting each time for a response
for request in range(10):
    print("Sending request %s …" % request)
    socket.send(json_data.encode('ascii'))

    #  Get the reply.
    message = socket.recv()
    print("Received reply %s [ %s ]" % (request, message))
