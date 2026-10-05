#include "game.hpp"

#include "card/share_deck.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

SingleGame::SingleGame(std::vector<FactionId> factions, unsigned seed)
    : map_(Map::autumn()), random_(seed) {
    if (factions.size() < 2 || factions.size() > 4) {
        throw std::invalid_argument("Root needs between two and four factions");
    }
    std::unordered_set<int> unique;
    for (const auto faction : factions) {
        if (faction == FactionId::Count || !unique.insert(static_cast<int>(faction)).second) {
            throw std::invalid_argument("Every faction in a game must be unique");
        }
        players_.push_back({Faction(faction), 0, {}});
    }
    reset();
}

SingleGame::PlayerState& SingleGame::player(FactionId faction) {
    auto found = std::find_if(players_.begin(), players_.end(), [faction](const auto& state) {
        return state.faction.id() == faction;
    });
    if (found == players_.end()) throw std::invalid_argument("Faction is not in this game");
    return *found;
}

const SingleGame::PlayerState& SingleGame::player(FactionId faction) const {
    return const_cast<SingleGame*>(this)->player(faction);
}

FactionId SingleGame::activeFaction() const {
    return players_.at(activePlayer_).faction.id();
}

int SingleGame::score(FactionId faction) const {
    return player(faction).score;
}

const std::vector<ShareCard>& SingleGame::hand(FactionId faction) const {
    return player(faction).hand;
}

void SingleGame::reset() {
    map_.clearPieces();
    drawPile_ = createShareDeck();
    std::shuffle(drawPile_.begin(), drawPile_.end(), random_);
    for (auto& state : players_) {
        state.score = 0;
        state.hand.clear();
        drawCards(state, 3);
    }
    activePlayer_ = 0;
    phase_ = Phase::Birdsong;
    actionsRemaining_ = 0;
    winner_.reset();
    setupFactions();
}

void SingleGame::setupFactions() {
    for (const auto& state : players_) {
        switch (state.faction.id()) {
            case FactionId::MarquiseDeCat:
                for (int id = 0; id < static_cast<int>(map_.size()); ++id) {
                    if (id != 9) map_.clearing(id).add(FactionId::MarquiseDeCat, PieceType::Warrior);
                }
                map_.clearing(0).add(FactionId::MarquiseDeCat, PieceType::Building);
                map_.clearing(0).add(FactionId::MarquiseDeCat, PieceType::Token);
                break;
            case FactionId::EyrieDynasties:
                map_.clearing(9).add(FactionId::EyrieDynasties, PieceType::Warrior, 6);
                map_.clearing(9).add(FactionId::EyrieDynasties, PieceType::Building);
                break;
            case FactionId::WoodlandAlliance:
            case FactionId::Vagabond:
            case FactionId::Count:
                break;
        }
    }
}

void SingleGame::advancePhase() {
    if (winner_) throw std::logic_error("The game is already over");
    if (phase_ == Phase::Birdsong) {
        phase_ = Phase::Daylight;
        actionsRemaining_ = 3;
    } else if (phase_ == Phase::Daylight) {
        phase_ = Phase::Evening;
        actionsRemaining_ = 0;
        drawCards(players_.at(activePlayer_), 1);
    } else {
        activePlayer_ = (activePlayer_ + 1) % players_.size();
        phase_ = Phase::Birdsong;
    }
}

void SingleGame::drawCards(PlayerState& state, int amount) {
    while (amount-- > 0 && !drawPile_.empty()) {
        state.hand.push_back(std::move(drawPile_.back()));
        drawPile_.pop_back();
    }
}

void SingleGame::requireAction(FactionId faction) const {
    if (winner_) throw std::logic_error("The game is already over");
    if (phase_ != Phase::Daylight) throw std::logic_error("Actions can only be taken in Daylight");
    if (activeFaction() != faction) throw std::logic_error("It is not this faction's turn");
    if (actionsRemaining_ <= 0) throw std::logic_error("No actions remain this turn");
}

void SingleGame::consumeAction() {
    --actionsRemaining_;
}

void SingleGame::move(FactionId faction, int from, int to, int warriors) {
    requireAction(faction);
    if (warriors <= 0) throw std::invalid_argument("Move at least one warrior");
    if (!map_.canMove(faction, from, to)) {
        throw std::invalid_argument("Move requires an adjacent road and rule at origin or destination");
    }
    auto& source = map_.clearing(from);
    if (source.pieces(faction, PieceType::Warrior) < warriors) {
        throw std::invalid_argument("Not enough warriors in the origin clearing");
    }
    source.remove(faction, PieceType::Warrior, warriors);
    map_.clearing(to).add(faction, PieceType::Warrior, warriors);
    consumeAction();
}

BattleResult SingleGame::battle(FactionId attacker, FactionId defender, int clearingId) {
    std::uniform_int_distribution<int> die(0, 3);
    return battle(attacker, defender, clearingId, die(random_), die(random_));
}

BattleResult SingleGame::battle(FactionId attacker, FactionId defender, int clearingId,
                                int firstRoll, int secondRoll) {
    requireAction(attacker);
    if (attacker == defender) throw std::invalid_argument("A faction cannot battle itself");
    if (firstRoll < 0 || firstRoll > 3 || secondRoll < 0 || secondRoll > 3) {
        throw std::invalid_argument("Root battle dice range from zero to three");
    }
    auto& land = map_.clearing(clearingId);
    const int attackerWarriors = land.pieces(attacker, PieceType::Warrior);
    const int defenderWarriors = land.pieces(defender, PieceType::Warrior);
    const int defenderPieces = defenderWarriors + land.pieces(defender, PieceType::Building)
        + land.pieces(defender, PieceType::Token);
    if (attackerWarriors == 0 || defenderPieces == 0) {
        throw std::invalid_argument("Both attacker and defender need pieces in the clearing");
    }

    BattleResult result;
    result.attackerRoll = std::max(firstRoll, secondRoll);
    result.defenderRoll = std::min(firstRoll, secondRoll);
    result.hitsToDefender = std::min(result.attackerRoll, attackerWarriors);
    result.hitsToAttacker = std::min(result.defenderRoll, defenderWarriors);
    if (defenderWarriors == 0) ++result.hitsToDefender;

    result.defenderCardboardRemoved = removeBattleLosses(defender, clearingId, result.hitsToDefender);
    result.attackerCardboardRemoved = removeBattleLosses(attacker, clearingId, result.hitsToAttacker);
    if (result.defenderCardboardRemoved) awardPoints(attacker, result.defenderCardboardRemoved);
    if (result.attackerCardboardRemoved) awardPoints(defender, result.attackerCardboardRemoved);
    consumeAction();
    return result;
}

int SingleGame::removeBattleLosses(FactionId faction, int clearingId, int hits) {
    auto& land = map_.clearing(clearingId);
    const int warriors = std::min(hits, land.pieces(faction, PieceType::Warrior));
    if (warriors) land.remove(faction, PieceType::Warrior, warriors);
    int remaining = hits - warriors;
    int cardboard = 0;
    for (const auto type : {PieceType::Token, PieceType::Building}) {
        const int removed = std::min(remaining, land.pieces(faction, type));
        if (removed) land.remove(faction, type, removed);
        remaining -= removed;
        cardboard += removed;
    }
    return cardboard;
}

void SingleGame::placePiece(FactionId faction, PieceType type, int clearingId, int amount) {
    player(faction);
    map_.clearing(clearingId).add(faction, type, amount);
}

void SingleGame::removePiece(FactionId faction, PieceType type, int clearingId, int amount) {
    player(faction);
    map_.clearing(clearingId).remove(faction, type, amount);
}

void SingleGame::awardPoints(FactionId faction, int points) {
    if (points < 0) throw std::invalid_argument("Points cannot be negative");
    auto& state = player(faction);
    state.score += points;
    if (state.score >= 30 && !winner_) winner_ = faction;
}
