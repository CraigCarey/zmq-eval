cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(zmq-eval VERSION 1.0.0 LANGUAGES CXX)

set(VERBOSE ON)
set(CMAKE_FIND_DEBUG_MODE ON)
set(CMAKE_CXX_STANDARD 17)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif (NOT CMAKE_BUILD_TYPE)

set(CMAKE_DEBUG_POSTFIX "-d")

set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

find_package(ZeroMQ REQUIRED)
find_package(cppzmq REQUIRED)

add_executable(hello_zmq_server hello_zmq_server.cpp)
target_link_libraries(hello_zmq_server cppzmq::cppzmq)

add_executable(hello_zmq_client hello_zmq_client.cpp)
target_link_libraries(hello_zmq_client cppzmq::cppzmq)

add_executable(encryption_demo encryption_demo.cpp)
target_link_libraries(encryption_demo cppzmq::cppzmq)
