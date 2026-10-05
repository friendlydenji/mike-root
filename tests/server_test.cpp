#include <cassert>
#include <iostream>
#include <string>

#include "../server/room_service.hpp"

std::string jsonValue(const std::string& body, const std::string& key) {
    const auto marker = "\"" + key + "\":\"";
    const auto start = body.find(marker);
    assert(start != std::string::npos);
    const auto valueStart = start + marker.size();
    const auto end = body.find('"', valueStart);
    assert(end != std::string::npos);
    return body.substr(valueStart, end - valueStart);
}

int main() {
    RoomService rooms;

    const auto created = rooms.handle("POST", "/api/rooms", {{"name", "Alice"}});
    assert(created.status == 201);
    const auto code = jsonValue(created.body, "code");
    const auto catToken = jsonValue(created.body, "token");

    const auto joined = rooms.handle("POST", "/api/rooms/join", {
        {"code", code}, {"name", "Bob"}
    });
    assert(joined.status == 200);
    const auto birdToken = jsonValue(joined.body, "token");
    assert(joined.body.find("\"status\":\"playing\"") != std::string::npos);
    assert(joined.body.find("\"deckCount\":48") != std::string::npos);
    assert(joined.body.find("\"hand\":[{") != std::string::npos);

    const auto invalidMove = rooms.handle("POST", "/api/action", {
        {"code", code}, {"token", catToken}, {"revision", "2"},
        {"action", "move"}, {"from", "0"}, {"to", "1"}, {"warriors", "1"}
    });
    assert(invalidMove.status == 409);

    const auto daylight = rooms.handle("POST", "/api/action", {
        {"code", code}, {"token", catToken}, {"revision", "2"},
        {"action", "advance_phase"}
    });
    assert(daylight.status == 200);
    assert(daylight.body.find("\"phase\":\"Daylight\"") != std::string::npos);

    const auto moved = rooms.handle("POST", "/api/action", {
        {"code", code}, {"token", catToken}, {"revision", "3"},
        {"action", "move"}, {"from", "0"}, {"to", "1"}, {"warriors", "1"}
    });
    assert(moved.status == 200);
    assert(moved.body.find("\"revision\":4") != std::string::npos);

    const auto wrongTurn = rooms.handle("POST", "/api/action", {
        {"code", code}, {"token", birdToken}, {"revision", "4"},
        {"action", "advance_phase"}
    });
    assert(wrongTurn.status == 409);

    const auto state = rooms.handle("GET", "/api/state", {
        {"code", code}, {"token", birdToken}
    });
    assert(state.status == 200);
    assert(state.body.find("Alice") != std::string::npos);
    assert(state.body.find("Bob") != std::string::npos);

    std::cout << "All multiplayer room tests passed.\n";
}
