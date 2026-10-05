#include "land.hpp"

#include <numeric>

Land::Land(int id, std::string name, Suit suit, int buildingSlots, bool ruin)
    : id_(id), name_(std::move(name)), suit_(suit), buildingSlots_(buildingSlots), hasRuin_(ruin) {
    if (id < 0 || buildingSlots < 0) {
        throw std::invalid_argument("A clearing needs a non-negative id and slot count");
    }
}

int Land::pieces(FactionId faction, PieceType type) const {
    const auto index = factionIndex(faction);
    switch (type) {
        case PieceType::Warrior: return warriors_.at(index);
        case PieceType::Building: return buildings_.at(index);
        case PieceType::Token: return tokens_.at(index);
    }
    throw std::logic_error("Unknown piece type");
}

int Land::ruleStrength(FactionId faction) const {
    const auto index = factionIndex(faction);
    return warriors_.at(index) + buildings_.at(index) + tokens_.at(index);
}

int Land::usedBuildingSlots() const {
    return std::accumulate(buildings_.begin(), buildings_.end(), 0);
}

void Land::add(FactionId faction, PieceType type, int amount) {
    if (amount <= 0) throw std::invalid_argument("Piece amount must be positive");
    if (type == PieceType::Building && usedBuildingSlots() + amount > buildingSlots_) {
        throw std::invalid_argument("The clearing has no free building slot");
    }
    auto& collection = type == PieceType::Warrior ? warriors_
        : type == PieceType::Building ? buildings_ : tokens_;
    collection.at(factionIndex(faction)) += amount;
}

void Land::remove(FactionId faction, PieceType type, int amount) {
    if (amount <= 0) throw std::invalid_argument("Piece amount must be positive");
    auto& collection = type == PieceType::Warrior ? warriors_
        : type == PieceType::Building ? buildings_ : tokens_;
    auto& count = collection.at(factionIndex(faction));
    if (count < amount) throw std::invalid_argument("Not enough pieces to remove");
    count -= amount;
}

void Land::clear() {
    warriors_.fill(0);
    buildings_.fill(0);
    tokens_.fill(0);
}
