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

std::size_t countOccurrences(const std::string& text, const std::string& needle) {
    std::size_t count = 0;
    for (std::size_t offset = 0; (offset = text.find(needle, offset)) != std::string::npos; offset += needle.size()) {
        ++count;
    }
    return count;
}

int main() {
    RoomService rooms;

    const auto created = rooms.handle("POST", "/api/rooms", {{"name", "Alice"}});
    assert(created.status == 201);
    const auto code = jsonValue(created.body, "code");
    const auto hostToken = jsonValue(created.body, "token");
    assert(created.body.find("\"status\":\"waiting\"") != std::string::npos);
    assert(created.body.find("\"isHost\":true") != std::string::npos);
    assert(created.body.find("\"you\":null") != std::string::npos);

    const auto joined = rooms.handle("POST", "/api/rooms/join", {
        {"code", code}, {"name", "Bob"}
    });
    assert(joined.status == 200);
    const auto guestToken = jsonValue(joined.body, "token");
    assert(joined.body.find("\"status\":\"waiting\"") != std::string::npos);
    assert(joined.body.find("\"faction\":null") != std::string::npos);

    const auto guestStart = rooms.handle("POST", "/api/rooms/start", {
        {"code", code}, {"token", guestToken}
    });
    assert(guestStart.status == 409);

    const auto started = rooms.handle("POST", "/api/rooms/start", {
        {"code", code}, {"token", hostToken}
    });
    assert(started.status == 200);
    assert(started.body.find("\"status\":\"setup\"") != std::string::npos);
    assert(started.body.find("\"phase\":\"Setup\"") != std::string::npos);
    assert(started.body.find("\"step\":\"place_keep\"") != std::string::npos);
    assert(started.body.find("\"deckCount\":44") != std::string::npos);
    assert(started.body.find("\"hand\":[{") != std::string::npos);

    const auto hostFaction = jsonValue(started.body, "you");
    assert(hostFaction == "Marquise de Cat" || hostFaction == "Eyrie Dynasties");

    std::uint64_t revision = 3;
    const auto catsToken = hostFaction == "Marquise de Cat" ? hostToken : guestToken;
    const auto eyrieToken = hostFaction == "Eyrie Dynasties" ? hostToken : guestToken;
    const auto setupAction = [&](const std::string& token, const std::string& action, int clearing) {
        return rooms.handle("POST", "/api/action", {
            {"code", code}, {"token", token}, {"revision", std::to_string(revision)},
            {"action", action}, {"clearing", std::to_string(clearing)}
        });
    };
    assert(setupAction(eyrieToken, "place_roost", 9).status == 409);
    assert(setupAction(catsToken, "advance_phase", 0).status == 409);
    assert(setupAction(catsToken, "produce_wood", 0).status == 409);
    assert(setupAction(catsToken, "place_keep", 4).status == 409);
    const auto keep = setupAction(catsToken, "place_keep", 0);
    assert(keep.status == 200);
    ++revision;
    assert(keep.body.find("\"step\":\"place_buildings\"") != std::string::npos);
    assert(setupAction(catsToken, "place_setup_sawmill", 0).status == 200);
    ++revision;
    assert(setupAction(catsToken, "place_setup_sawmill", 1).status == 409);
    assert(setupAction(catsToken, "place_setup_workshop", 0).status == 409);
    assert(setupAction(catsToken, "place_setup_workshop", 1).status == 200);
    ++revision;
    assert(setupAction(catsToken, "place_setup_recruiter", 3).status == 200);
    ++revision;
    const auto roostState = rooms.handle("GET", "/api/state", {
        {"code", code}, {"token", eyrieToken}
    });
    assert(roostState.status == 200);
    assert(roostState.body.find("\"phaseActions\":[\"place_roost\"]") != std::string::npos);
    assert(countOccurrences(roostState.body, "\"actions\":[\"place_roost\"]") == 1);
    assert(setupAction(eyrieToken, "place_roost", 2).status == 409);
    const auto roost = setupAction(eyrieToken, "place_roost", 9);
    assert(roost.status == 200);
    assert(roost.body.find("\"phaseActions\":[\"choose_eyrie_leader\"]") != std::string::npos);
    ++revision;
    const auto setupLeader = rooms.handle("POST", "/api/action", {
        {"code", code}, {"token", eyrieToken}, {"revision", std::to_string(revision)},
        {"action", "choose_eyrie_leader"}, {"leader", "Builder"}
    });
    assert(setupLeader.status == 200);
    ++revision;
    assert(setupLeader.body.find("\"status\":\"playing\"") != std::string::npos);
    assert(setupLeader.body.find("\"phase\":\"Birdsong\"") != std::string::npos);
    assert(setupAction(catsToken, "place_keep", 2).status == 409);
    if (hostFaction == "Eyrie Dynasties") {
        const auto decree = rooms.handle("POST", "/api/action", {
            {"code", code}, {"token", hostToken}, {"revision", std::to_string(revision)},
            {"action", "add_to_decree"}, {"card", "0"}, {"column", "Battle"}
        });
        assert(decree.status == 200);
        assert(decree.body.find("\"type\":\"eyrie\"") != std::string::npos);
        ++revision;
    }

    const auto invalidMove = rooms.handle("POST", "/api/action", {
        {"code", code}, {"token", hostToken}, {"revision", std::to_string(revision)},
        {"action", "move"}, {"from", "0"}, {"to", "1"}, {"warriors", "1"}
    });
    assert(invalidMove.status == 409);

    if (hostFaction == "Marquise de Cat") {
        const auto blockedAdvance = rooms.handle("POST", "/api/action", {
            {"code", code}, {"token", hostToken}, {"revision", std::to_string(revision)}, {"action", "advance_phase"}
        });
        assert(blockedAdvance.status == 409);
        const auto wood = rooms.handle("POST", "/api/action", {
            {"code", code}, {"token", hostToken}, {"revision", std::to_string(revision)}, {"action", "produce_wood"}
        });
        assert(wood.status == 200);
        assert(wood.body.find("\"woodProduced\":true") != std::string::npos);
        ++revision;
    }

    const auto daylight = rooms.handle("POST", "/api/action", {
        {"code", code}, {"token", hostToken}, {"revision", std::to_string(revision)},
        {"action", "advance_phase"}
    });
    assert(daylight.status == 200);
    assert(daylight.body.find("\"phase\":\"Daylight\"") != std::string::npos);
    ++revision;
    if (hostFaction == "Eyrie Dynasties") {
        const auto recruited = rooms.handle("POST", "/api/action", {
            {"code", code}, {"token", hostToken}, {"revision", std::to_string(revision)},
            {"action", "recruit"}, {"clearing", "9"}
        });
        assert(recruited.status == 200);
        ++revision;
    }

    const auto moved = rooms.handle("POST", "/api/action", {
        {"code", code}, {"token", hostToken}, {"revision", std::to_string(revision)},
        {"action", "move"},
        {"from", hostFaction == "Marquise de Cat" ? "0" : "9"},
        {"to", hostFaction == "Marquise de Cat" ? "1" : "6"}, {"warriors", "1"}
    });
    assert(moved.status == 200);
    ++revision;
    assert(moved.body.find("\"revision\":" + std::to_string(revision)) != std::string::npos);

    const auto wrongTurn = rooms.handle("POST", "/api/action", {
        {"code", code}, {"token", guestToken}, {"revision", std::to_string(revision)},
        {"action", "advance_phase"}
    });
    assert(wrongTurn.status == 409);

    const auto state = rooms.handle("GET", "/api/state", {
        {"code", code}, {"token", guestToken}
    });
    assert(state.status == 200);
    assert(state.body.find("Alice") != std::string::npos);
    assert(state.body.find("Bob") != std::string::npos);

    RoomService fourPlayerRooms;
    const auto fourCreated = fourPlayerRooms.handle("POST", "/api/rooms", {{"name", "Host"}});
    const auto fourCode = jsonValue(fourCreated.body, "code");
    const auto fourHostToken = jsonValue(fourCreated.body, "token");
    for (const auto* name : {"Two", "Three", "Four"}) {
        const auto response = fourPlayerRooms.handle("POST", "/api/rooms/join", {
            {"code", fourCode}, {"name", name}
        });
        assert(response.status == 200);
        assert(response.body.find("\"status\":\"waiting\"") != std::string::npos);
    }
    const auto fourStarted = fourPlayerRooms.handle("POST", "/api/rooms/start", {
        {"code", fourCode}, {"token", fourHostToken}
    });
    assert(fourStarted.status == 200);
    assert(fourStarted.body.find("\"status\":\"setup\"") != std::string::npos);
    for (const auto* faction : {
        "Marquise de Cat", "Eyrie Dynasties", "Woodland Alliance", "Vagabond"
    }) {
        assert(fourStarted.body.find(faction) != std::string::npos);
    }
    const auto lateJoin = fourPlayerRooms.handle("POST", "/api/rooms/join", {
        {"code", fourCode}, {"name", "Late"}
    });
    assert(lateJoin.status == 409);

    std::cout << "All multiplayer room tests passed.\n";
}
