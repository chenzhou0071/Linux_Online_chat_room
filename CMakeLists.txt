cmake_minimum_required(VERSION 3.10)
project(ChatServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR})

add_executable(chat_server
    server/main.cpp
    server/EpollServer.cpp
    server/WebSocket.cpp
    server/ThreadPool.cpp
)

target_link_libraries(chat_server pthread ssl crypto)
