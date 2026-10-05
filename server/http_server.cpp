#include "http_server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace {
struct Request {
    std::string method;
    std::string target;
    std::map<std::string, std::string> headers;
    std::string body;
};

std::string urlDecode(const std::string& value) {
    std::string decoded;
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '+') decoded.push_back(' ');
        else if (value[index] == '%' && index + 2 < value.size()) {
            const auto hex = value.substr(index + 1, 2);
            decoded.push_back(static_cast<char>(std::stoi(hex, nullptr, 16)));
            index += 2;
        } else decoded.push_back(value[index]);
    }
    return decoded;
}

RoomService::Parameters parseParameters(const std::string& encoded) {
    RoomService::Parameters parameters;
    std::size_t start = 0;
    while (start <= encoded.size()) {
        const auto end = encoded.find('&', start);
        const auto part = encoded.substr(start, end == std::string::npos ? end : end - start);
        const auto separator = part.find('=');
        if (!part.empty()) {
            parameters[urlDecode(part.substr(0, separator))] = separator == std::string::npos
                ? "" : urlDecode(part.substr(separator + 1));
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return parameters;
}

bool sendAll(int socket, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const auto result = ::send(socket, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (result <= 0) return false;
        sent += static_cast<std::size_t>(result);
    }
    return true;
}

Request readRequest(int socket) {
    constexpr std::size_t MaxRequestSize = 1024 * 1024;
    std::string data;
    std::array<char, 8192> buffer{};
    while (data.find("\r\n\r\n") == std::string::npos) {
        const auto count = recv(socket, buffer.data(), buffer.size(), 0);
        if (count <= 0) throw std::runtime_error("Connection closed");
        data.append(buffer.data(), static_cast<std::size_t>(count));
        if (data.size() > MaxRequestSize) throw std::runtime_error("Request is too large");
    }

    const auto headerEnd = data.find("\r\n\r\n");
    std::istringstream stream(data.substr(0, headerEnd));
    Request request;
    std::string version;
    stream >> request.method >> request.target >> version;
    std::string line;
    std::getline(stream, line);
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto separator = line.find(':');
        if (separator == std::string::npos) continue;
        auto key = line.substr(0, separator);
        for (auto& character : key) character = static_cast<char>(std::tolower(character));
        auto value = line.substr(separator + 1);
        if (!value.empty() && value.front() == ' ') value.erase(value.begin());
        request.headers[key] = value;
    }

    std::size_t contentLength = 0;
    if (const auto found = request.headers.find("content-length"); found != request.headers.end()) {
        contentLength = static_cast<std::size_t>(std::stoul(found->second));
    }
    request.body = data.substr(headerEnd + 4);
    while (request.body.size() < contentLength) {
        const auto count = recv(socket, buffer.data(), buffer.size(), 0);
        if (count <= 0) throw std::runtime_error("Incomplete request body");
        request.body.append(buffer.data(), static_cast<std::size_t>(count));
    }
    request.body.resize(contentLength);
    return request;
}

std::string statusText(int status) {
    if (status == 200) return "OK";
    if (status == 201) return "Created";
    if (status == 400) return "Bad Request";
    if (status == 404) return "Not Found";
    if (status == 409) return "Conflict";
    return "Internal Server Error";
}

void sendResponse(int socket, int status, const std::string& contentType, const std::string& body) {
    std::ostringstream response;
    response << "HTTP/1.1 " << status << ' ' << statusText(status) << "\r\n"
             << "Content-Type: " << contentType << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Cache-Control: no-store\r\n"
             << "Connection: close\r\n\r\n" << body;
    sendAll(socket, response.str());
}

std::string mimeType(const std::string& path) {
    const auto endsWith = [&path](const std::string& suffix) {
        return path.size() >= suffix.size()
            && path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    if (endsWith(".css")) return "text/css; charset=utf-8";
    if (endsWith(".js")) return "text/javascript; charset=utf-8";
    if (endsWith(".json")) return "application/json; charset=utf-8";
    if (endsWith(".png")) return "image/png";
    if (endsWith(".webp")) return "image/webp";
    if (endsWith(".jpg") || endsWith(".jpeg")) return "image/jpeg";
    if (endsWith(".svg")) return "image/svg+xml";
    return "text/html; charset=utf-8";
}
}

HttpServer::HttpServer(int port, std::string webRoot)
    : port_(port), webRoot_(std::move(webRoot)) {}

void HttpServer::run() {
    const int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) throw std::runtime_error(std::strerror(errno));
    int reuse = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(static_cast<std::uint16_t>(port_));
    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0
        || listen(serverSocket, 64) < 0) {
        close(serverSocket);
        throw std::runtime_error(std::strerror(errno));
    }
    std::cout << "Root server listening on http://127.0.0.1:" << port_ << '\n';
    while (true) {
        const int client = accept(serverSocket, nullptr, nullptr);
        if (client >= 0) std::thread(&HttpServer::serveClient, this, client).detach();
    }
}

void HttpServer::serveClient(int socket) {
    try {
        const auto request = readRequest(socket);
        const auto queryAt = request.target.find('?');
        const auto path = request.target.substr(0, queryAt);
        auto parameters = parseParameters(queryAt == std::string::npos
            ? "" : request.target.substr(queryAt + 1));
        if (request.method == "POST") {
            const auto bodyParameters = parseParameters(request.body);
            parameters.insert(bodyParameters.begin(), bodyParameters.end());
        }

        if (path.compare(0, 5, "/api/") == 0) {
            const auto response = rooms_.handle(request.method, path, parameters);
            sendResponse(socket, response.status, "application/json; charset=utf-8", response.body);
        } else {
            const bool projectAsset = path.compare(0, 8, "/assets/") == 0;
            std::string asset = path == "/" ? "index.html"
                : projectAsset ? path.substr(8) : path.substr(1);
            if (asset.find("..") != std::string::npos) throw std::invalid_argument("Invalid path");
            const auto root = projectAsset
                ? std::filesystem::path(webRoot_).parent_path() / "assets"
                : std::filesystem::path(webRoot_);
            const auto filePath = root / asset;
            std::ifstream file(filePath, std::ios::binary);
            if (!file) sendResponse(socket, 404, "text/plain", "Not found");
            else {
                std::ostringstream content;
                content << file.rdbuf();
                sendResponse(socket, 200, mimeType(asset), content.str());
            }
        }
    } catch (const std::exception& error) {
        sendResponse(socket, 400, "application/json; charset=utf-8",
                     "{\"ok\":false,\"error\":\"Bad request\"}");
        std::cerr << "Request error: " << error.what() << '\n';
    }
    close(socket);
}
