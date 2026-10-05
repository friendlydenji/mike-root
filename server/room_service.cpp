#include "room_service.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

namespace {
std::string jsonEscape(const std::string& value) {
    std::ostringstream escaped;
    for (const unsigned char character : value) {
        switch (character) {
            case '"': escaped << "\\\""; break;
            case '\\': escaped << "\\\\"; break;
            case '\n': escaped << "\\n"; break;
            case '\r': escaped << "\\r"; break;
            case '\t': escaped << "\\t"; break;
            default:
                if (character < 0x20) {
                    escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                            << static_cast<int>(character) << std::dec;
                } else {
                    escaped << character;
                }
        }
    }
    return escaped.str();
}

std::string randomText(std::size_t length, const std::string& alphabet) {
    static std::mutex randomMutex;
    static std::mt19937 generator(std::random_device{}());
    std::lock_guard lock(randomMutex);
    std::uniform_int_distribution<std::size_t> distribution(0, alphabet.size() - 1);
    std::string result;
    result.reserve(length);
    while (result.size() < length) result.push_back(alphabet[distribution(generator)]);
    return result;
}

std::string roomCode() {
    return randomText(6, "ABCDEFGHJKLMNPQRSTUVWXYZ23456789");
}

std::string playerToken() {
    return randomText(32, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
}

std::string cleanName(const std::string& value) {
    std::string name;
    for (const unsigned char character : value) {
        if (character >= 0x20 && name.size() < 24) name.push_back(character);
    }
    if (name.empty()) throw std::invalid_argument("Player name is required");
    return name;
}

int integerParameter(const RoomService::Parameters& parameters, const std::string& key,
                     int minimum, int maximum) {
    const auto found = parameters.find(key);
    if (found == parameters.end()) throw std::invalid_argument("Missing parameter: " + key);
    std::size_t consumed = 0;
    int value = 0;
    try {
        value = std::stoi(found->second, &consumed);
    } catch (const std::exception&) {
        throw std::invalid_argument("Invalid number: " + key);
    }
    if (consumed != found->second.size() || value < minimum || value > maximum) {
        throw std::invalid_argument("Number out of range: " + key);
    }
    return value;
}

const char* suitName(Suit suit) {
    switch (suit) {
        case Suit::Fox: return "fox";
        case Suit::Mouse: return "mouse";
        case Suit::Rabbit: return "rabbit";
        case Suit::Bird: return "bird";
        case Suit::Any: return "any";
    }
    return "unknown";
}

const char* phaseName(Phase phase) {
    switch (phase) {
        case Phase::Birdsong: return "Birdsong";
        case Phase::Daylight: return "Daylight";
        case Phase::Evening: return "Evening";
    }
    return "Unknown";
}

std::string errorBody(const std::string& message) {
    return "{\"ok\":false,\"error\":\"" + jsonEscape(message) + "\"}";
}
}

ApiResponse RoomService::handle(const std::string& method, const std::string& path,
                                const Parameters& parameters) {
    std::lock_guard lock(mutex_);
    try {
        if (method == "POST" && path == "/api/rooms") return createRoom(parameters);
        if (method == "POST" && path == "/api/rooms/join") return joinRoom(parameters);
        if (method == "GET" && path == "/api/state") return getState(parameters);
        if (method == "POST" && path == "/api/action") return performAction(parameters);
        return {404, errorBody("API route not found")};
    } catch (const std::out_of_range& error) {
        return {404, errorBody(error.what())};
    } catch (const std::logic_error& error) {
        return {409, errorBody(error.what())};
    } catch (const std::exception& error) {
        return {400, errorBody(error.what())};
    }
}

ApiResponse RoomService::createRoom(const Parameters& parameters) {
    const auto name = cleanName(parameters.count("name") ? parameters.at("name") : "");
    std::string code;
    do { code = roomCode(); } while (rooms_.count(code));

    Room room;
    room.code = code;
    room.players.push_back({name, playerToken(), FactionId::MarquiseDeCat});
    const auto token = room.players.front().token;
    rooms_.emplace(code, std::move(room));
    const auto& saved = rooms_.at(code);
    return {201, "{\"ok\":true,\"token\":\"" + token + "\",\"state\":"
        + serializeRoom(saved, saved.players.front()) + "}"};
}

ApiResponse RoomService::joinRoom(const Parameters& parameters) {
    auto& room = requireRoom(parameters);
    if (room.players.size() >= 2) throw std::logic_error("Room is already full");
    const auto name = cleanName(parameters.count("name") ? parameters.at("name") : "");
    room.players.push_back({name, playerToken(), FactionId::EyrieDynasties});
    room.game = std::make_unique<SingleGame>(
        std::vector<FactionId>{FactionId::MarquiseDeCat, FactionId::EyrieDynasties});
    room.lastEvent = name + " joined. The game is ready.";
    ++room.revision;
    const auto& player = room.players.back();
    return {200, "{\"ok\":true,\"token\":\"" + player.token + "\",\"state\":"
        + serializeRoom(room, player) + "}"};
}

ApiResponse RoomService::getState(const Parameters& parameters) {
    auto& room = requireRoom(parameters);
    const auto& player = requirePlayer(room, parameters);
    return {200, "{\"ok\":true,\"state\":" + serializeRoom(room, player) + "}"};
}

ApiResponse RoomService::performAction(const Parameters& parameters) {
    auto& room = requireRoom(parameters);
    const auto& player = requirePlayer(room, parameters);
    if (!room.game) throw std::logic_error("Wait for another player before starting");
    const auto actionIt = parameters.find("action");
    if (actionIt == parameters.end()) throw std::invalid_argument("Action is required");

    if (const auto revision = parameters.find("revision"); revision != parameters.end()) {
        const auto expected = static_cast<std::uint64_t>(std::stoull(revision->second));
        if (expected != room.revision) throw std::logic_error("Game state changed; refresh and try again");
    }

    if (actionIt->second == "move") {
        const int from = integerParameter(parameters, "from", 0, 11);
        const int to = integerParameter(parameters, "to", 0, 11);
        const int warriors = integerParameter(parameters, "warriors", 1, 25);
        room.game->move(player.faction, from, to, warriors);
        room.lastEvent = player.name + " moved " + std::to_string(warriors) + " warrior(s).";
    } else if (actionIt->second == "battle") {
        const int clearing = integerParameter(parameters, "clearing", 0, 11);
        const auto defender = player.faction == FactionId::MarquiseDeCat
            ? FactionId::EyrieDynasties : FactionId::MarquiseDeCat;
        const auto result = room.game->battle(player.faction, defender, clearing);
        room.lastEvent = player.name + " battled: " + std::to_string(result.hitsToDefender)
            + " hit(s) dealt, " + std::to_string(result.hitsToAttacker) + " received.";
    } else if (actionIt->second == "advance_phase") {
        if (room.game->activeFaction() != player.faction) {
            throw std::logic_error("It is not your turn");
        }
        room.game->advancePhase();
        room.lastEvent = player.name + " advanced the turn phase.";
    } else {
        throw std::invalid_argument("Unknown action");
    }

    ++room.revision;
    return {200, "{\"ok\":true,\"state\":" + serializeRoom(room, player) + "}"};
}

RoomService::Room& RoomService::requireRoom(const Parameters& parameters) {
    const auto found = parameters.find("code");
    if (found == parameters.end()) throw std::invalid_argument("Room code is required");
    std::string code = found->second;
    std::transform(code.begin(), code.end(), code.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    const auto room = rooms_.find(code);
    if (room == rooms_.end()) throw std::out_of_range("Room not found");
    return room->second;
}

const RoomService::Player& RoomService::requirePlayer(
    const Room& room, const Parameters& parameters) {
    const auto token = parameters.find("token");
    if (token == parameters.end()) throw std::invalid_argument("Player token is required");
    const auto player = std::find_if(room.players.begin(), room.players.end(), [&](const auto& entry) {
        return entry.token == token->second;
    });
    if (player == room.players.end()) throw std::logic_error("Player token is invalid");
    return *player;
}

std::string RoomService::serializeRoom(const Room& room, const Player& viewer) {
    std::ostringstream json;
    json << "{\"code\":\"" << room.code << "\",\"revision\":" << room.revision
         << ",\"status\":\"" << (room.game ? "playing" : "waiting") << "\""
         << ",\"you\":\"" << factionName(viewer.faction) << "\""
         << ",\"lastEvent\":\"" << jsonEscape(room.lastEvent) << "\",\"players\":[";
    for (std::size_t index = 0; index < room.players.size(); ++index) {
        if (index) json << ',';
        const auto& player = room.players[index];
        json << "{\"name\":\"" << jsonEscape(player.name) << "\",\"faction\":\""
             << factionName(player.faction) << "\"}";
    }
    json << ']';
    if (!room.game) return json.str() + '}';

    const auto& game = *room.game;
    json << ",\"phase\":\"" << phaseName(game.phase()) << "\",\"activeFaction\":\""
         << factionName(game.activeFaction()) << "\",\"actionsRemaining\":"
         << game.actionsRemaining() << ",\"scores\":{\"Marquise de Cat\":"
         << game.score(FactionId::MarquiseDeCat) << ",\"Eyrie Dynasties\":"
         << game.score(FactionId::EyrieDynasties) << "},\"deckCount\":" << game.deckSize()
         << ",\"hand\":[";
    const auto& hand = game.hand(viewer.faction);
    for (std::size_t index = 0; index < hand.size(); ++index) {
        if (index) json << ',';
        json << "{\"title\":\"" << jsonEscape(hand[index].getCardName())
             << "\",\"suit\":\"" << suitName(hand[index].getCardSuit()) << "\"}";
    }
    json << "],\"winner\":";
    if (game.winner()) json << '"' << factionName(*game.winner()) << '"';
    else json << "null";

    json << ",\"clearings\":[";
    for (int id = 0; id < static_cast<int>(game.map().size()); ++id) {
        if (id) json << ',';
        const auto& land = game.map().clearing(id);
        json << "{\"id\":" << id << ",\"name\":\"" << jsonEscape(land.name())
             << "\",\"suit\":\"" << suitName(land.suit()) << "\",\"slots\":"
             << land.buildingSlots() << ",\"ruin\":" << (land.hasRuin() ? "true" : "false")
             << ",\"ruler\":";
        if (const auto ruler = game.map().ruler(id)) json << '"' << factionName(*ruler) << '"';
        else json << "null";
        json << ",\"pieces\":{\"Marquise de Cat\":{"
             << "\"warriors\":" << land.pieces(FactionId::MarquiseDeCat, PieceType::Warrior)
             << ",\"buildings\":" << land.pieces(FactionId::MarquiseDeCat, PieceType::Building)
             << ",\"tokens\":" << land.pieces(FactionId::MarquiseDeCat, PieceType::Token)
             << "},\"Eyrie Dynasties\":{"
             << "\"warriors\":" << land.pieces(FactionId::EyrieDynasties, PieceType::Warrior)
             << ",\"buildings\":" << land.pieces(FactionId::EyrieDynasties, PieceType::Building)
             << ",\"tokens\":" << land.pieces(FactionId::EyrieDynasties, PieceType::Token)
             << "}},\"neighbors\":[";
        const auto& neighbors = game.map().neighbors(id);
        for (std::size_t index = 0; index < neighbors.size(); ++index) {
            if (index) json << ',';
            json << neighbors[index];
        }
        json << "]}";
    }
    json << "]}";
    return json.str();
}
