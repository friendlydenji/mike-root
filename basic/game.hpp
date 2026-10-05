#pragma once

#include <optional>
#include <random>
#include <string>
#include <vector>

#include "card/share_card.hpp"
#include "faction/faction.hpp"
#include "map/map.hpp"

struct BattleResult {
    int attackerRoll{};
    int defenderRoll{};
    int hitsToDefender{};
    int hitsToAttacker{};
    int defenderCardboardRemoved{};
    int attackerCardboardRemoved{};
};

class SingleGame {
public:
    explicit SingleGame(std::vector<FactionId> factions, unsigned seed = std::random_device{}());

    const Map& map() const noexcept { return map_; }
    Map& map() noexcept { return map_; }
    Phase phase() const noexcept { return phase_; }
    FactionId activeFaction() const;
    int actionsRemaining() const noexcept { return actionsRemaining_; }
    int score(FactionId faction) const;
    std::optional<FactionId> winner() const noexcept { return winner_; }
    const std::vector<ShareCard>& hand(FactionId faction) const;
    std::size_t deckSize() const noexcept { return drawPile_.size(); }

    void reset();
    void advancePhase();
    void move(FactionId faction, int from, int to, int warriors);
    BattleResult battle(FactionId attacker, FactionId defender, int clearingId);
    BattleResult battle(FactionId attacker, FactionId defender, int clearingId,
                        int firstRoll, int secondRoll);
    void placePiece(FactionId faction, PieceType type, int clearingId, int amount = 1);
    void removePiece(FactionId faction, PieceType type, int clearingId, int amount = 1);
    void awardPoints(FactionId faction, int points);

private:
    struct PlayerState {
        Faction faction;
        int score{};
        std::vector<ShareCard> hand;
    };

    Map map_;
    std::vector<PlayerState> players_;
    std::size_t activePlayer_{};
    Phase phase_{Phase::Birdsong};
    int actionsRemaining_{};
    std::optional<FactionId> winner_;
    std::mt19937 random_;
    std::vector<ShareCard> drawPile_;

    PlayerState& player(FactionId faction);
    const PlayerState& player(FactionId faction) const;
    void requireAction(FactionId faction) const;
    void consumeAction();
    void setupFactions();
    void drawCards(PlayerState& player, int amount);
    int removeBattleLosses(FactionId faction, int clearingId, int hits);
};
