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

std::vector<FactionId> baseFactionMix(std::size_t playerCount) {
    if (playerCount < 2 || playerCount > 4) {
        throw std::logic_error("Root needs between two and four players");
    }

    std::vector<FactionId> factions{FactionId::MarquiseDeCat, FactionId::EyrieDynasties};
    if (playerCount == 3) {
        std::uniform_int_distribution<int> choice(0, 1);
        std::mt19937 generator(std::random_device{}());
        factions.push_back(choice(generator) == 0
            ? FactionId::WoodlandAlliance : FactionId::Vagabond);
    } else if (playerCount == 4) {
        factions.push_back(FactionId::WoodlandAlliance);
        factions.push_back(FactionId::Vagabond);
    }

    std::mt19937 generator(std::random_device{}());
    std::shuffle(factions.begin(), factions.end(), generator);
    return factions;
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
        case Phase::Setup: return "Setup";
        case Phase::Birdsong: return "Birdsong";
        case Phase::Daylight: return "Daylight";
        case Phase::Evening: return "Evening";
    }
    return "Unknown";
}

constexpr std::array<FactionId, 4> BaseFactions{
    FactionId::MarquiseDeCat,
    FactionId::EyrieDynasties,
    FactionId::WoodlandAlliance,
    FactionId::Vagabond
};

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
        if (method == "POST" && path == "/api/rooms/start") return startRoom(parameters);
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
    room.players.push_back({name, playerToken(), std::nullopt});
    const auto token = room.players.front().token;
    room.hostToken = token;
    rooms_.emplace(code, std::move(room));
    const auto& saved = rooms_.at(code);
    return {201, "{\"ok\":true,\"token\":\"" + token + "\",\"state\":"
        + serializeRoom(saved, saved.players.front()) + "}"};
}

ApiResponse RoomService::joinRoom(const Parameters& parameters) {
    auto& room = requireRoom(parameters);
    if (room.game) throw std::logic_error("The game has already started");
    if (room.players.size() >= 4) throw std::logic_error("Room is already full");
    const auto name = cleanName(parameters.count("name") ? parameters.at("name") : "");
    room.players.push_back({name, playerToken(), std::nullopt});
    room.lastEvent = name + " joined the room.";
    ++room.revision;
    const auto& player = room.players.back();
    return {200, "{\"ok\":true,\"token\":\"" + player.token + "\",\"state\":"
        + serializeRoom(room, player) + "}"};
}

ApiResponse RoomService::startRoom(const Parameters& parameters) {
    auto& room = requireRoom(parameters);
    const auto& host = requirePlayer(room, parameters);
    if (host.token != room.hostToken) throw std::logic_error("Only the host can start the game");
    if (room.game) throw std::logic_error("The game has already started");
    if (room.players.size() < 2) throw std::logic_error("At least two players are required");

    auto factions = baseFactionMix(room.players.size());
    for (std::size_t index = 0; index < room.players.size(); ++index) {
        room.players[index].faction = factions[index];
    }
    room.game = std::make_unique<SingleGame>(factions);
    room.lastEvent = host.name + " started the game. Factions were assigned randomly.";
    ++room.revision;
    return {200, "{\"ok\":true,\"state\":" + serializeRoom(room, host) + "}"};
}

ApiResponse RoomService::getState(const Parameters& parameters) {
    auto& room = requireRoom(parameters);
    const auto& player = requirePlayer(room, parameters);
    return {200, "{\"ok\":true,\"state\":" + serializeRoom(room, player) + "}"};
}

ApiResponse RoomService::performAction(const Parameters& parameters) {
    auto& room = requireRoom(parameters);
    const auto& player = requirePlayer(room, parameters);
    if (!room.game || !player.faction) throw std::logic_error("Wait for the host to start the game");
    const auto actionIt = parameters.find("action");
    if (actionIt == parameters.end()) throw std::invalid_argument("Action is required");

    if (const auto revision = parameters.find("revision"); revision != parameters.end()) {
        std::size_t consumed = 0;
        const auto expected = static_cast<std::uint64_t>(std::stoull(revision->second, &consumed));
        if (consumed != revision->second.size()) throw std::logic_error("Invalid revision");
        if (expected != room.revision) throw std::logic_error("Game state changed; refresh and try again");
    }

    if ((actionIt->second == "rule_action" || actionIt->second == "respond") && !parameters.count("revision")) {
        throw std::logic_error("A revision is required for this choice");
    }
    if ((!room.game->decisions().empty() || room.game->pendingBattle()) && actionIt->second != "respond") {
        throw std::logic_error("Resolve the pending response first");
    }
    // Invalid input must not leave a partially modified multiplayer room.
    struct Transaction {
        Room& room;
        SingleGame game;
        std::string event;
        std::optional<BattleEvent> battle;
        bool committed;
        ~Transaction() {
            if (!committed) { *room.game = std::move(game); room.lastEvent = std::move(event); room.lastBattle = battle; }
        }
    } transaction{room, *room.game, room.lastEvent, room.lastBattle, false};

    if (actionIt->second == "respond") {
        const auto choice = parameters.find("choice");
        if (choice == parameters.end()) throw std::logic_error("A response choice is required");
        room.game->respond(*player.faction, choice->second);
        room.lastEvent = player.name + " resolved a response.";
    } else if (actionIt->second == "rule_action") {
        room.game->executeRuleAction(*player.faction, static_cast<std::size_t>(integerParameter(parameters, "index", 0, 10000)));
        room.lastEvent = player.name + " used a faction or card action.";
    } else if (actionIt->second == "new_roost") {
        room.game->newRoost(*player.faction, integerParameter(parameters, "clearing", 0, 11));
        room.lastEvent = player.name + " placed A New Roost.";
    } else if (actionIt->second == "place_keep") {
        room.game->placeSetupKeep(*player.faction, integerParameter(parameters, "clearing", 0, 11));
        room.lastEvent = player.name + " placed the Keep and starting warriors.";
    } else if (actionIt->second == "place_roost") {
        room.game->placeSetupRoost(*player.faction, integerParameter(parameters, "clearing", 0, 11));
        room.lastEvent = player.name + " placed the starting roost and six warriors.";
    } else if (actionIt->second == "place_setup_sawmill" || actionIt->second == "place_setup_workshop"
               || actionIt->second == "place_setup_recruiter") {
        const auto type = actionIt->second == "place_setup_sawmill" ? CatBuilding::Sawmill
            : actionIt->second == "place_setup_workshop" ? CatBuilding::Workshop : CatBuilding::Recruiter;
        room.game->placeSetupCatBuilding(*player.faction, type, integerParameter(parameters, "clearing", 0, 11));
        room.lastEvent = player.name + " placed a starting " + actionIt->second.substr(12) + ".";
    } else if (actionIt->second == "move") {
        const int from = integerParameter(parameters, "from", 0, *player.faction == FactionId::Vagabond ? 18 : 11);
        const int to = integerParameter(parameters, "to", 0, 11);
        const int warriors = integerParameter(parameters, "warriors", 1, 25);
        room.game->move(*player.faction, from, to, warriors);
        room.lastEvent = player.name + " moved " + std::to_string(warriors) + " warrior(s).";
    } else if (actionIt->second == "battle") {
        const int clearing = integerParameter(parameters, "clearing", 0, 11);
        const auto& land = room.game->map().clearing(clearing);
        const auto defender = std::find_if(room.players.begin(), room.players.end(), [&](const auto& candidate) {
            if (!candidate.faction || candidate.faction == player.faction) return false;
            const auto requested = parameters.find("defender");
            if (requested != parameters.end() && factionName(*candidate.faction) != requested->second) return false;
            return land.pieces(*candidate.faction, PieceType::Warrior)
                + land.pieces(*candidate.faction, PieceType::Building)
                + land.pieces(*candidate.faction, PieceType::Token) > 0;
        });
        if (defender == room.players.end()) {
            throw std::logic_error("There is no enemy faction to battle in this clearing");
        }
        room.game->beginBattle(*player.faction, *defender->faction, clearing);
        room.lastEvent = player.name + " initiated a battle.";
    } else if (actionIt->second == "advance_phase") {
        if (room.game->activeFaction() != *player.faction) {
            throw std::logic_error("It is not your turn");
        }
        room.game->advancePhase();
        room.lastEvent = player.name + " advanced the turn phase.";
    } else if (actionIt->second == "draw_card") {
        room.game->drawCard(*player.faction);
        room.lastEvent = player.name + " drew a card.";
    } else if (actionIt->second == "spread_sympathy") {
        const int clearing = integerParameter(parameters, "clearing", 0, 11);
        room.game->spreadSympathy(*player.faction, clearing);
        room.lastEvent = player.name + " spread sympathy.";
    } else if (actionIt->second == "revolt") {
        const int clearing = integerParameter(parameters, "clearing", 0, 11);
        room.game->revolt(*player.faction, clearing);
        room.lastEvent = player.name + " revolted and established a base.";
    } else if (actionIt->second == "mobilize") {
        const int card = integerParameter(parameters, "card", 0, 99);
        room.game->mobilize(*player.faction, static_cast<std::size_t>(card));
        room.lastEvent = player.name + " mobilized a supporter.";
    } else if (actionIt->second == "train") {
        const int card = integerParameter(parameters, "card", 0, 99);
        room.game->train(*player.faction, static_cast<std::size_t>(card));
        room.lastEvent = player.name + " trained an officer.";
    } else if (actionIt->second == "recruit") {
        const int clearing = *player.faction == FactionId::MarquiseDeCat ? 0
            : integerParameter(parameters, "clearing", 0, 11);
        room.game->recruit(*player.faction, clearing);
        room.lastEvent = player.name + " recruited warriors.";
    } else if (actionIt->second == "produce_wood") {
        room.game->produceWood(*player.faction);
        room.lastEvent = player.name + " produced wood at sawmills.";
    } else if (actionIt->second == "build_sawmill" || actionIt->second == "build_workshop"
               || actionIt->second == "build_recruiter") {
        const int clearing = integerParameter(parameters, "clearing", 0, 11);
        const auto type = actionIt->second == "build_sawmill" ? CatBuilding::Sawmill
            : actionIt->second == "build_workshop" ? CatBuilding::Workshop : CatBuilding::Recruiter;
        room.game->buildCatBuilding(*player.faction, type, clearing);
        room.lastEvent = player.name + " built a " + actionIt->second.substr(6) + ".";
    } else if (actionIt->second == "overwork") {
        const int clearing = integerParameter(parameters, "clearing", 0, 11);
        const int card = integerParameter(parameters, "card", 0, 99);
        room.game->overwork(*player.faction, clearing, static_cast<std::size_t>(card));
        room.lastEvent = player.name + " overworked a sawmill.";
    } else if (actionIt->second == "spend_bird") {
        const int card = integerParameter(parameters, "card", 0, 99);
        room.game->spendBirdCard(*player.faction, static_cast<std::size_t>(card));
        room.lastEvent = player.name + " spent a bird card for an extra action.";
    } else if (actionIt->second == "finish_march") {
        room.game->finishMarch(*player.faction);
        room.lastEvent = player.name + " finished the March.";
    } else if (actionIt->second == "discard_card") {
        const int card = integerParameter(parameters, "card", 0, 99);
        room.game->discardCard(*player.faction, static_cast<std::size_t>(card));
        room.lastEvent = player.name + " discarded a card.";
    } else if (actionIt->second == "craft") {
        const int card = integerParameter(parameters, "card", 0, 99);
        room.game->craft(*player.faction, static_cast<std::size_t>(card));
        room.lastEvent = player.name + " crafted an item.";
    } else if (actionIt->second == "build_roost") {
        const int clearing = integerParameter(parameters, "clearing", 0, 11);
        room.game->buildRoost(*player.faction, clearing);
        room.lastEvent = player.name + " built an Eyrie roost.";
    } else if (actionIt->second == "organize") {
        const int clearing = integerParameter(parameters, "clearing", 0, 11);
        room.game->organize(*player.faction, clearing);
        room.lastEvent = player.name + " organized a sympathetic clearing.";
    } else if (actionIt->second == "slip") {
        const int to = integerParameter(parameters, "to", 0, 18);
        room.game->slip(*player.faction, to);
        room.lastEvent = player.name + " slipped into an adjacent clearing.";
    } else if (actionIt->second == "explore") {
        const int clearing = integerParameter(parameters, "clearing", 0, 11);
        const auto item = room.game->explore(*player.faction, clearing);
        room.lastEvent = player.name + " explored a ruin and found a " + item + ".";
    } else if (actionIt->second == "aid") {
        const int clearing = integerParameter(parameters, "clearing", 0, 11);
        const auto& land = room.game->map().clearing(clearing);
        const auto target = std::find_if(room.players.begin(), room.players.end(), [&](const auto& candidate) {
            if (!candidate.faction || candidate.faction == player.faction) return false;
            return land.pieces(*candidate.faction, PieceType::Warrior)
                + land.pieces(*candidate.faction, PieceType::Building)
                + land.pieces(*candidate.faction, PieceType::Token) > 0;
        });
        if (target == room.players.end()) throw std::logic_error("There is no faction to Aid here");
        room.game->aid(*player.faction, *target->faction, clearing);
        room.lastEvent = player.name + " aided " + target->name + ".";
    } else if (actionIt->second == "repair") {
        room.game->repair(*player.faction);
        room.lastEvent = player.name + " repaired a damaged item.";
    } else if (actionIt->second == "choose_eyrie_leader") {
        const auto leader = parameters.find("leader");
        if (leader == parameters.end()) throw std::invalid_argument("Leader is required");
        room.game->chooseEyrieLeader(*player.faction, leader->second);
        room.lastEvent = player.name + " chose the " + leader->second + " leader.";
    } else if (actionIt->second == "add_to_decree") {
        const int card = integerParameter(parameters, "card", 0, 99);
        const auto column = parameters.find("column");
        if (column == parameters.end()) throw std::invalid_argument("Decree column is required");
        room.game->addToDecree(*player.faction, static_cast<std::size_t>(card), column->second);
        room.lastEvent = player.name + " added a card to the " + column->second + " Decree.";
    } else {
        throw std::invalid_argument("Unknown action");
    }

    if (!room.game->pendingBattle() && room.game->completedBattle()
        && (!transaction.game.completedBattle()
            || transaction.game.completedBattle()->id != room.game->completedBattle()->id)) {
        const auto& battle = *room.game->completedBattle();
        room.lastBattle = BattleEvent{room.revision + 1, battle.attacker, battle.defender, battle.clearing, battle.result};
    }
    transaction.committed = true;
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
         << ",\"status\":\"" << (!room.game ? "waiting" : room.game->phase() == Phase::Setup ? "setup" : "playing") << "\""
         << ",\"isHost\":" << (viewer.token == room.hostToken ? "true" : "false")
         << ",\"maxPlayers\":4,\"you\":";
    if (viewer.faction) json << '"' << factionName(*viewer.faction) << '"';
    else json << "null";
    json
         << ",\"lastEvent\":\"" << jsonEscape(room.lastEvent) << "\",\"players\":[";
    for (std::size_t index = 0; index < room.players.size(); ++index) {
        if (index) json << ',';
        const auto& player = room.players[index];
        json << "{\"name\":\"" << jsonEscape(player.name) << "\",\"faction\":";
        if (player.faction) json << '"' << factionName(*player.faction) << '"';
        else json << "null";
        json << ",\"host\":" << (player.token == room.hostToken ? "true" : "false") << '}';
    }
    json << ']';
    if (!room.game) return json.str() + '}';

    const auto& game = *room.game;
    const auto& setup = game.setupState();
    json << ",\"setup\":{\"step\":\"" << setup.step << "\",\"faction\":";
    if (setup.faction) json << '"' << factionName(*setup.faction) << '"';
    else json << "null";
    json << ",\"keep\":";
    if (setup.keep) json << *setup.keep; else json << "null";
    json << ",\"eyrieCorner\":";
    if (setup.eyrieCorner) json << *setup.eyrieCorner; else json << "null";
    json << ",\"buildingsPlaced\":{\"sawmill\":" << (setup.buildingsPlaced[0] ? "true" : "false")
         << ",\"workshop\":" << (setup.buildingsPlaced[1] ? "true" : "false")
         << ",\"recruiter\":" << (setup.buildingsPlaced[2] ? "true" : "false") << "}}";
    json << ",\"phase\":\"" << phaseName(game.phase()) << "\",\"activeFaction\":\""
         << factionName(game.activeFaction()) << "\",\"actionsRemaining\":"
         << game.actionsRemaining() << ",\"canDraw\":"
         << (game.canDrawCard(*viewer.faction) ? "true" : "false")
         << ",\"canAdvance\":" << (game.canAdvancePhase(*viewer.faction) ? "true" : "false")
         << ",\"scores\":{";
    for (std::size_t index = 0; index < room.players.size(); ++index) {
        if (index) json << ',';
        const auto faction = *room.players[index].faction;
        json << '"' << factionName(faction) << "\":" << game.score(faction);
    }
    json << "},\"deckCount\":" << game.deckSize()
         << ",\"discardCount\":" << game.discardSize()
         << ",\"hand\":[";
    const auto& hand = game.hand(*viewer.faction);
    for (std::size_t index = 0; index < hand.size(); ++index) {
        if (index) json << ',';
        json << "{\"title\":\"" << jsonEscape(hand[index].getCardName())
             << "\",\"suit\":\"" << suitName(hand[index].getCardSuit()) << "\",\"actions\":[";
        const auto cardActions = game.cardActions(*viewer.faction, index);
        for (std::size_t action = 0; action < cardActions.size(); ++action) {
            if (action) json << ',';
            json << '"' << cardActions[action] << '"';
        }
        json << "]}";
    }
    json << "],\"phaseActions\":[";
    const auto phaseActions = game.phaseActions(*viewer.faction);
    for (std::size_t index = 0; index < phaseActions.size(); ++index) {
        if (index) json << ',';
        json << '"' << phaseActions[index] << '"';
    }
    json << "],\"craftedItems\":{";
    bool firstItem = true;
    for (const auto& [item, count] : game.craftedItems(*viewer.faction)) {
        if (!firstItem) json << ',';
        firstItem = false;
        json << '"' << item << "\":" << count;
    }
    json << "},\"factionState\":";
    if (*viewer.faction == FactionId::MarquiseDeCat) {
        const auto& cat = game.catState();
        json << "{\"type\":\"cat\",\"woodProduced\":" << (cat.woodProduced ? "true" : "false")
             << ",\"recruited\":" << (cat.recruited ? "true" : "false")
             << ",\"marchMovesRemaining\":" << cat.marchMovesRemaining
             << ",\"wood\":" << game.catWoodCount() << ",\"buildingCounts\":{";
        bool firstBuilding = true;
        for (const auto type : {CatBuilding::Sawmill, CatBuilding::Workshop, CatBuilding::Recruiter}) {
            const char* name = type == CatBuilding::Sawmill ? "sawmill" : type == CatBuilding::Workshop ? "workshop" : "recruiter";
            if (!firstBuilding) json << ',';
            firstBuilding = false;
            json << '"' << name << "\":" << game.catBuildingCount(type);
        }
        json << "},\"buildCosts\":{\"sawmill\":" << game.catBuildCost(CatBuilding::Sawmill)
             << ",\"workshop\":" << game.catBuildCost(CatBuilding::Workshop)
             << ",\"recruiter\":" << game.catBuildCost(CatBuilding::Recruiter) << "}}";
    } else if (*viewer.faction == FactionId::EyrieDynasties) {
        const auto& eyrie = game.eyrieState();
        json << "{\"type\":\"eyrie\",\"leader\":";
        if (eyrie.leader.empty()) json << "null";
        else json << '"' << jsonEscape(eyrie.leader) << '"';
        json << ",\"cardsAdded\":" << eyrie.cardsAddedThisBirdsong
             << ",\"birdAdded\":" << (eyrie.birdAddedThisBirdsong ? "true" : "false")
             << ",\"needsLeaderAfterTurmoil\":" << (eyrie.needsLeaderAfterTurmoil ? "true" : "false")
             << ",\"currentColumn\":\"" << game.currentDecreeColumn()
             << "\",\"deposedLeaders\":[";
        for (std::size_t index = 0; index < eyrie.deposedLeaders.size(); ++index) {
            if (index) json << ',';
            json << '"' << jsonEscape(eyrie.deposedLeaders[index]) << '"';
        }
        json << ']'
             << ",\"decree\":{";
        constexpr std::array<const char*, 4> columns{"Recruit", "Move", "Battle", "Build"};
        for (std::size_t columnIndex = 0; columnIndex < columns.size(); ++columnIndex) {
            if (columnIndex) json << ',';
            json << '"' << columns[columnIndex] << "\":[";
            const auto& cards = eyrie.decree[columnIndex];
            for (std::size_t cardIndex = 0; cardIndex < cards.size(); ++cardIndex) {
                if (cardIndex) json << ',';
                json << "{\"title\":\"" << jsonEscape(cards[cardIndex].title)
                     << "\",\"suit\":\"" << suitName(cards[cardIndex].suit)
                     << "\",\"resolved\":" << (cards[cardIndex].resolved ? "true" : "false") << '}';
            }
            json << ']';
        }
        json << "}}";
    } else if (*viewer.faction == FactionId::WoodlandAlliance) {
        json << "{\"type\":\"alliance\",\"officers\":"
             << game.officers(*viewer.faction) << ",\"sympathyPlaced\":"
             << game.sympathyPlaced() << ",\"supporters\":[";
        const auto& supporters = game.supporters(*viewer.faction);
        for (std::size_t index = 0; index < supporters.size(); ++index) {
            if (index) json << ',';
            json << "{\"title\":\"" << jsonEscape(supporters[index].getCardName())
                 << "\",\"suit\":\"" << suitName(supporters[index].getCardSuit()) << "\"}";
        }
        json << "]}";
    } else if (*viewer.faction == FactionId::Vagabond) {
        const auto& items = game.vagabondItems();
        json << "{\"type\":\"vagabond\",\"items\":{"
             << "\"boots\":" << items.boots << ",\"swords\":" << items.swords
             << ",\"torches\":" << items.torches << ",\"hammers\":" << items.hammers
             << ",\"tea\":" << items.tea << ",\"exhaustedBoots\":" << items.exhaustedBoots
             << ",\"exhaustedSwords\":" << items.exhaustedSwords
             << ",\"exhaustedTorches\":" << items.exhaustedTorches
             << ",\"exhaustedHammers\":" << items.exhaustedHammers
             << ",\"exhaustedTea\":" << items.exhaustedTea
             << ",\"damaged\":" << items.damaged << "},\"relationships\":{";
        bool first = true;
        for (const auto& roomPlayer : room.players) {
            if (!roomPlayer.faction || *roomPlayer.faction == FactionId::Vagabond) continue;
            if (!first) json << ',';
            first = false;
            json << '"' << factionName(*roomPlayer.faction) << "\":"
                 << game.relationship(*roomPlayer.faction);
        }
        json << "}}";
    } else {
        json << "{\"type\":\"standard\"}";
    }
    json << ",\"ruleActions\":[";
    const auto rules = game.ruleActions(*viewer.faction);
    for (std::size_t index = 0; index < rules.size(); ++index) {
        if (index) json << ',';
        const auto& rule = rules[index];
        json << "{\"index\":" << index << ",\"type\":\"" << rule.type << "\",\"label\":\"" << jsonEscape(rule.label)
             << "\",\"card\":" << rule.card << ",\"item\":" << rule.item << ",\"clearing\":" << rule.clearing
             << ",\"target\":\"" << factionName(rule.target) << "\",\"option\":\"" << jsonEscape(rule.option) << "\"}";
    }
    json << "],\"persistentCards\":[";
    const auto& effects = game.persistentCards(*viewer.faction);
    for (std::size_t index = 0; index < effects.size(); ++index) {
        if (index) json << ',';
        json << "{\"title\":\"" << jsonEscape(effects[index].getCardName()) << "\",\"suit\":\"" << suitName(effects[index].getCardSuit()) << "\"}";
    }
    json << "],\"response\":";
    if (!game.decisions().empty()) {
        const auto& decision = game.decisions().front();
        const bool yours = decision.faction == *viewer.faction;
        json << "{\"faction\":\"" << factionName(decision.faction) << "\",\"kind\":\"" << decision.kind
             << "\",\"title\":\"" << jsonEscape(yours ? decision.title : "Waiting for a player response") << "\",\"choices\":[";
        if (yours) for (std::size_t index = 0; index < decision.choices.size(); ++index) {
            if (index) json << ',';
            json << "{\"value\":\"" << jsonEscape(decision.choices[index].value) << "\",\"label\":\"" << jsonEscape(decision.choices[index].label) << "\"}";
        }
        json << "]}";
    } else json << "null";
    json << ",\"pendingBattle\":";
    if (game.pendingBattle()) {
        const auto& battle = *game.pendingBattle();
        json << "{\"attacker\":\"" << factionName(battle.attacker) << "\",\"defender\":\"" << factionName(battle.defender)
             << "\",\"clearing\":" << battle.clearing << ",\"attackerRoll\":" << battle.result.attackerRoll
             << ",\"defenderRoll\":" << battle.result.defenderRoll << '}';
    } else json << "null";
    json << ",\"inventory\":[";
    for (std::size_t index = 0; index < game.inventory().size(); ++index) {
        if (index) json << ',';
        const auto& item = game.inventory()[index];
        json << "{\"id\":" << item.id << ",\"type\":\"" << item.type << "\",\"exhausted\":" << (item.exhausted ? "true" : "false")
             << ",\"damaged\":" << (item.damaged ? "true" : "false") << '}';
    }
    json << "],\"character\":\"" << game.vagabondCharacter() << "\",\"vagabondLocation\":" << game.vagabondLocation()
         << ",\"quests\":[";
    for (std::size_t index = 0; index < game.quests().size(); ++index) {
        if (index) json << ',';
        json << "{\"id\":\"" << game.quests()[index].value << "\",\"art\":\"" << game.quests()[index].label << "\"}";
    }
    json << "],\"forests\":[";
    for (std::size_t index = 0; index < game.forests().size(); ++index) {
        if (index) json << ',';
        json << "{\"id\":" << index + 12 << ",\"clearings\":[";
        for (std::size_t boundary = 0; boundary < game.forests()[index].size(); ++boundary) {
            if (boundary) json << ',';
            json << game.forests()[index][boundary];
        }
        json << "]}";
    }
    json << "],\"victoryConditions\":{";
    for (std::size_t index = 0; index < room.players.size(); ++index) {
        if (index) json << ',';
        const auto faction = *room.players[index].faction;
        json << '"' << factionName(faction) << "\":";
        if (game.dominance(faction)) json << '"' << suitName(*game.dominance(faction)) << '"';
        else json << "null";
    }
    json << "},\"coalition\":";
    if (game.coalition()) json << '"' << factionName(*game.coalition()) << '"'; else json << "null";
    json << ",\"lastBattle\":";
    if (room.lastBattle) {
        const auto& battle = *room.lastBattle;
        json << "{\"revision\":" << battle.revision
             << ",\"attacker\":\"" << factionName(battle.attacker)
             << "\",\"defender\":\"" << factionName(battle.defender)
             << "\",\"clearing\":" << battle.clearing
             << ",\"attackerRoll\":" << battle.result.attackerRoll
             << ",\"defenderRoll\":" << battle.result.defenderRoll
             << ",\"hitsToDefender\":" << battle.result.hitsToDefender
             << ",\"hitsToAttacker\":" << battle.result.hitsToAttacker
             << ",\"rolled\":" << (battle.result.diceRolled ? "true" : "false")
             << ",\"ambushHits\":" << battle.result.ambushHits << '}';
    } else {
        json << "null";
    }
    json << ",\"winner\":";
    if (game.winner()) json << '"' << factionName(*game.winner()) << '"';
    else json << "null";

    json << ",\"clearings\":[";
    for (int id = 0; id < static_cast<int>(game.map().size()); ++id) {
        if (id) json << ',';
        const auto& land = game.map().clearing(id);
        json << "{\"id\":" << id << ",\"name\":\"" << jsonEscape(land.name())
             << "\",\"suit\":\"" << suitName(land.suit()) << "\",\"slots\":"
             << land.buildingSlots() << ",\"ruin\":" << (land.hasRuin() ? "true" : "false")
             << ",\"sympathyCost\":" << game.sympathyCost(id)
             << ",\"catBuildings\":{\"sawmill\":" << land.catBuildings(CatBuilding::Sawmill)
             << ",\"workshop\":" << land.catBuildings(CatBuilding::Workshop)
             << ",\"recruiter\":" << land.catBuildings(CatBuilding::Recruiter)
             << "},\"wood\":" << land.wood() << ",\"keep\":" << land.keep()
             << ",\"ruler\":";
        if (const auto ruler = game.map().ruler(id)) json << '"' << factionName(*ruler) << '"';
        else json << "null";
        json << ",\"pieces\":{";
        for (std::size_t factionIndex = 0; factionIndex < BaseFactions.size(); ++factionIndex) {
            if (factionIndex) json << ',';
            const auto faction = BaseFactions[factionIndex];
            json << '"' << factionName(faction) << "\":{\"warriors\":"
                 << land.pieces(faction, PieceType::Warrior) << ",\"buildings\":"
                 << land.pieces(faction, PieceType::Building) << ",\"tokens\":"
                 << land.pieces(faction, PieceType::Token) << '}';
        }
        json << "},\"neighbors\":[";
        const auto& neighbors = game.map().neighbors(id);
        for (std::size_t index = 0; index < neighbors.size(); ++index) {
            if (index) json << ',';
            json << neighbors[index];
        }
        json << "],\"actions\":[";
        const auto options = game.clearingOptions(*viewer.faction, id);
        for (std::size_t index = 0; index < options.actions.size(); ++index) {
            if (index) json << ',';
            json << '"' << options.actions[index] << '"';
        }
        json << "],\"moveTargets\":[";
        for (std::size_t index = 0; index < options.moveTargets.size(); ++index) {
            if (index) json << ',';
            json << options.moveTargets[index];
        }
        json << "],\"slipTargets\":[";
        for (std::size_t index = 0; index < options.slipTargets.size(); ++index) {
            if (index) json << ',';
            json << options.slipTargets[index];
        }
        json << "],\"battleTargets\":[";
        for (std::size_t index = 0; index < options.battleTargets.size(); ++index) {
            if (index) json << ',';
            json << '"' << factionName(options.battleTargets[index]) << '"';
        }
        json << "]}";
    }
    json << "]}";
    return json.str();
}
