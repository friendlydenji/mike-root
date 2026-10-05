#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "game.hpp"

struct ApiResponse {
    int status{200};
    std::string body;
};

class RoomService {
public:
    using Parameters = std::map<std::string, std::string>;

    ApiResponse handle(const std::string& method, const std::string& path,
                       const Parameters& parameters);

private:
    struct Player {
        std::string name;
        std::string token;
        FactionId faction;
    };

    struct Room {
        std::string code;
        std::vector<Player> players;
        std::unique_ptr<SingleGame> game;
        std::uint64_t revision{1};
        std::string lastEvent{"Waiting for another player"};
    };

    std::mutex mutex_;
    std::unordered_map<std::string, Room> rooms_;

    ApiResponse createRoom(const Parameters& parameters);
    ApiResponse joinRoom(const Parameters& parameters);
    ApiResponse getState(const Parameters& parameters);
    ApiResponse performAction(const Parameters& parameters);
    Room& requireRoom(const Parameters& parameters);
    static const Player& requirePlayer(const Room& room, const Parameters& parameters);
    static std::string serializeRoom(const Room& room, const Player& viewer);
};
