#include "land.hpp"

#include <numeric>
#include <algorithm>

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
    if (faction == FactionId::Vagabond) return 0;
    const auto index = factionIndex(faction);
    return warriors_.at(index) + buildings_.at(index);
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
    if (faction == FactionId::MarquiseDeCat) {
        if (type == PieceType::Token) wood_ -= std::min(wood_, amount);
        if (type == PieceType::Building) {
            int remaining = amount;
            for (auto& buildings : catBuildings_) {
                const int removed = std::min(buildings, remaining);
                buildings -= removed;
                remaining -= removed;
            }
        }
    }
    count -= amount;
}

int Land::catBuildings(CatBuilding type) const {
    return catBuildings_.at(static_cast<std::size_t>(type));
}

void Land::addCatBuilding(CatBuilding type) {
    const auto index = static_cast<std::size_t>(type);
    catBuildings_.at(index);
    add(FactionId::MarquiseDeCat, PieceType::Building);
    ++catBuildings_[index];
}

void Land::addWood(int amount) {
    add(FactionId::MarquiseDeCat, PieceType::Token, amount);
    wood_ += amount;
}

void Land::removeCatBuilding(CatBuilding type) {
    auto& count = catBuildings_.at(static_cast<std::size_t>(type));
    if (count <= 0) throw std::logic_error("No building of that type to remove");
    --count;
    --buildings_.at(factionIndex(FactionId::MarquiseDeCat));
}

void Land::removeKeep() {
    if (keep() <= 0) throw std::logic_error("No Keep to remove");
    --tokens_.at(factionIndex(FactionId::MarquiseDeCat));
}

void Land::removeWood(int amount) {
    if (amount <= 0 || amount > wood_) throw std::invalid_argument("Not enough wood to remove");
    remove(FactionId::MarquiseDeCat, PieceType::Token, amount);
}

void Land::clear() {
    warriors_.fill(0);
    buildings_.fill(0);
    tokens_.fill(0);
    catBuildings_.fill(0);
    wood_ = 0;
}
