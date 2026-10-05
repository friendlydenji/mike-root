#include "http_server.hpp"

#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    try {
        const int port = argc > 1 ? std::stoi(argv[1]) : 8080;
        const std::string webRoot = argc > 2 ? argv[2] : "web";
        HttpServer(port, webRoot).run();
    } catch (const std::exception& error) {
        std::cerr << "Server failed: " << error.what() << '\n';
        return 1;
    }
}
