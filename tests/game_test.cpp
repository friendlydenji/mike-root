#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "card/share_deck.hpp"
#include "game.hpp"

template <typename Action>
void expectFailure(Action action) {
    bool failed = false;
    try { action(); } catch (const std::exception&) { failed = true; }
    assert(failed);
}

int main() {
    const auto sharedDeck = createShareDeck();
    assert(sharedDeck.size() == 54);
    for (const auto& card : sharedDeck) assert(!card.getCardName().empty());

    Map map = Map::autumn();
    assert(map.size() == 12);
    assert(map.areAdjacent(0, 1));
    assert(!map.areAdjacent(0, 9));

    map.clearing(4).add(FactionId::MarquiseDeCat, PieceType::Warrior, 2);
    map.clearing(4).add(FactionId::EyrieDynasties, PieceType::Warrior, 2);
    assert(map.ruler(4) == FactionId::EyrieDynasties);

    SingleGame game({FactionId::MarquiseDeCat, FactionId::EyrieDynasties}, 7);
    assert(game.hand(FactionId::MarquiseDeCat).size() == 3);
    assert(game.hand(FactionId::EyrieDynasties).size() == 3);
    assert(game.deckSize() == 48);
    assert(game.phase() == Phase::Birdsong);
    expectFailure([&] { game.move(FactionId::MarquiseDeCat, 0, 1, 1); });
    game.advancePhase();
    assert(game.actionsRemaining() == 3);
    game.move(FactionId::MarquiseDeCat, 0, 1, 1);
    assert(game.map().clearing(1).pieces(FactionId::MarquiseDeCat, PieceType::Warrior) == 2);
    assert(game.actionsRemaining() == 2);

    game.placePiece(FactionId::EyrieDynasties, PieceType::Warrior, 1, 2);
    const auto battle = game.battle(FactionId::MarquiseDeCat, FactionId::EyrieDynasties, 1, 3, 1);
    assert(battle.hitsToDefender == 2);
    assert(battle.hitsToAttacker == 1);
    assert(game.map().clearing(1).pieces(FactionId::EyrieDynasties, PieceType::Warrior) == 0);
    game.advancePhase();
    assert(game.phase() == Phase::Evening);
    assert(game.hand(FactionId::MarquiseDeCat).size() == 4);
    assert(game.deckSize() == 47);

    game.awardPoints(FactionId::MarquiseDeCat, 30);
    assert(game.winner() == FactionId::MarquiseDeCat);
    expectFailure([&] { game.advancePhase(); });

    std::cout << "All Root core logic tests passed.\n";
}
