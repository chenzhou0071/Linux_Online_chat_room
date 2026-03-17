#include <iostream>
#include "EpollServer.h"

int main(int argc, char* argv[]) {
    int port = 8080;
    int thread_count = 4;

    if (argc >= 2) {
        port = std::stoi(argv[1]);
    }
    if (argc >= 3) {
        thread_count = std::stoi(argv[2]);
    }

    std::cout << "Starting Chat Server..." << std::endl;
    std::cout << "Port: " << port << std::endl;
    std::cout << "Thread pool size: " << thread_count << std::endl;

    EpollServer server(port, thread_count);
    server.start();

    return 0;
}
