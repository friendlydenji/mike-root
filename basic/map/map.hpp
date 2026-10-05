#pragma once

#include <optional>
#include <vector>

#include "land.hpp"

class Map {
public:
    static Map autumn();

    int addClearing(const std::string& name, Suit suit, int buildingSlots, bool ruin = false);
    void connect(int first, int second);
    const Land& clearing(int id) const;
    Land& clearing(int id);
    bool areAdjacent(int first, int second) const;
    const std::vector<int>& neighbors(int id) const;
    std::optional<FactionId> ruler(int clearingId) const;
    bool rules(FactionId faction, int clearingId) const;
    bool canMove(FactionId faction, int from, int to) const;
    std::size_t size() const noexcept { return clearings_.size(); }
    void clearPieces();

private:
    std::vector<Land> clearings_;
    std::vector<std::vector<int>> roads_;
};
