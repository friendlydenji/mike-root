#pragma once

#include <array>
#include <stdexcept>
#include <string>

#include "../types.hpp"

class Land {
public:
    static constexpr std::size_t FactionCount = factionIndex(FactionId::Count);

    Land(int id, std::string name, Suit suit, int buildingSlots, bool ruin = false);

    int id() const noexcept { return id_; }
    const std::string& name() const noexcept { return name_; }
    Suit suit() const noexcept { return suit_; }
    int buildingSlots() const noexcept { return buildingSlots_; }
    bool hasRuin() const noexcept { return hasRuin_; }

    int pieces(FactionId faction, PieceType type) const;
    int ruleStrength(FactionId faction) const;
    int usedBuildingSlots() const;
    void add(FactionId faction, PieceType type, int amount = 1);
    void remove(FactionId faction, PieceType type, int amount = 1);
    void clear();

private:
    int id_;
    std::string name_;
    Suit suit_;
    int buildingSlots_;
    bool hasRuin_;
    std::array<int, FactionCount> warriors_{};
    std::array<int, FactionCount> buildings_{};
    std::array<int, FactionCount> tokens_{};
};
