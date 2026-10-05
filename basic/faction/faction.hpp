#pragma once

#include <string_view>

#include "../types.hpp"

class Faction {
public:
    explicit Faction(FactionId id) : id_(id) {}
    FactionId id() const noexcept { return id_; }
    std::string_view name() const noexcept { return factionName(id_); }
    bool winsRuleTies() const noexcept { return id_ == FactionId::EyrieDynasties; }

private:
    FactionId id_;
};
