#include "map.hpp"

#include <algorithm>
#include <stdexcept>

int Map::addClearing(const std::string& name, Suit suit, int buildingSlots, bool ruin) {
    const int id = static_cast<int>(clearings_.size());
    clearings_.emplace_back(id, name, suit, buildingSlots, ruin);
    roads_.emplace_back();
    return id;
}

void Map::connect(int first, int second) {
    clearing(first);
    clearing(second);
    if (first == second) throw std::invalid_argument("A clearing cannot connect to itself");
    auto addUnique = [](std::vector<int>& roads, int destination) {
        if (std::find(roads.begin(), roads.end(), destination) == roads.end()) roads.push_back(destination);
    };
    addUnique(roads_.at(first), second);
    addUnique(roads_.at(second), first);
}

const Land& Map::clearing(int id) const {
    if (id < 0 || static_cast<std::size_t>(id) >= clearings_.size()) {
        throw std::out_of_range("Unknown clearing");
    }
    return clearings_.at(static_cast<std::size_t>(id));
}

Land& Map::clearing(int id) {
    return const_cast<Land&>(static_cast<const Map&>(*this).clearing(id));
}

bool Map::areAdjacent(int first, int second) const {
    const auto& links = neighbors(first);
    return std::find(links.begin(), links.end(), second) != links.end();
}

const std::vector<int>& Map::neighbors(int id) const {
    clearing(id);
    return roads_.at(static_cast<std::size_t>(id));
}

std::optional<FactionId> Map::ruler(int clearingId) const {
    const auto& land = clearing(clearingId);
    int bestStrength = 0;
    std::optional<FactionId> best;
    bool tied = false;
    for (std::size_t index = 0; index < factionIndex(FactionId::Count); ++index) {
        const auto faction = static_cast<FactionId>(index);
        const int strength = land.ruleStrength(faction);
        if (strength > bestStrength) {
            bestStrength = strength;
            best = faction;
            tied = false;
        } else if (strength > 0 && strength == bestStrength) {
            if (faction == FactionId::EyrieDynasties) {
                best = faction;
                tied = false;
            } else if (best != FactionId::EyrieDynasties) {
                tied = true;
            }
        }
    }
    return tied ? std::nullopt : best;
}

bool Map::rules(FactionId faction, int clearingId) const {
    return ruler(clearingId) == faction;
}

bool Map::canMove(FactionId faction, int from, int to) const {
    return areAdjacent(from, to) &&
        clearing(from).pieces(faction, PieceType::Warrior) > 0 &&
        (rules(faction, from) || rules(faction, to));
}

void Map::clearPieces() {
    for (auto& land : clearings_) land.clear();
}

Map Map::autumn() {
    Map map;
    map.addClearing("Northwest", Suit::Fox, 1);
    map.addClearing("North", Suit::Rabbit, 2);
    map.addClearing("Northeast", Suit::Mouse, 2);
    map.addClearing("West", Suit::Mouse, 2);
    map.addClearing("Central West", Suit::Rabbit, 2, true);
    map.addClearing("Central East", Suit::Mouse, 3, true);
    map.addClearing("East", Suit::Fox, 2, true);
    map.addClearing("Southwest", Suit::Rabbit, 1);
    map.addClearing("South", Suit::Fox, 2, true);
    map.addClearing("Southeast", Suit::Rabbit, 1);
    map.addClearing("Lower West", Suit::Fox, 2);
    map.addClearing("Lower East", Suit::Mouse, 2);

    const int roads[][2] = {
        {0,1},{0,3},{0,4},{1,2},{2,4},{2,6},{3,7},{3,8},{4,8},
        {5,6},{5,8},{5,9},{6,9},{7,8},{7,10},{8,11},{9,11},{10,11}
    };
    for (const auto& road : roads) map.connect(road[0], road[1]);
    return map;
}
