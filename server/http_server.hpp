#pragma once

#include <string>

#include "room_service.hpp"

class HttpServer {
public:
    HttpServer(int port, std::string webRoot);
    void run();

private:
    int port_;
    std::string webRoot_;
    RoomService rooms_;

    void serveClient(int socket);
};
