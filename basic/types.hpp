#pragma once

#include <cstddef>
#include <string_view>

enum class Suit { Fox, Mouse, Rabbit, Bird, Any };

enum class FactionId {
    MarquiseDeCat,
    EyrieDynasties,
    WoodlandAlliance,
    Vagabond,
    Count
};

enum class PieceType { Warrior, Building, Token };
enum class Phase { Birdsong, Daylight, Evening };

constexpr std::size_t factionIndex(FactionId faction) {
    return static_cast<std::size_t>(faction);
}

constexpr std::string_view factionName(FactionId faction) {
    switch (faction) {
        case FactionId::MarquiseDeCat: return "Marquise de Cat";
        case FactionId::EyrieDynasties: return "Eyrie Dynasties";
        case FactionId::WoodlandAlliance: return "Woodland Alliance";
        case FactionId::Vagabond: return "Vagabond";
        case FactionId::Count: break;
    }
    return "Unknown";
}
