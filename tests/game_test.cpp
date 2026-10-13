#include <algorithm>
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

    const std::array<int, 4> corners{0, 2, 7, 9};
    const std::array<int, 4> opposite{9, 7, 2, 0};
    const std::array<std::string, 3> placements{"place_setup_sawmill", "place_setup_workshop", "place_setup_recruiter"};
    for (std::size_t cornerIndex = 0; cornerIndex < corners.size(); ++cornerIndex) {
        SingleGame setupGame({FactionId::EyrieDynasties, FactionId::MarquiseDeCat}, 31);
        assert(setupGame.phase() == Phase::Setup);
        assert(setupGame.activeFaction() == FactionId::MarquiseDeCat);
        assert(setupGame.catBuildingCount(CatBuilding::Sawmill) == 0);
        assert(setupGame.phaseActions(FactionId::EyrieDynasties).empty());
        for (int id = 0; id < 12; ++id) {
            assert(setupGame.map().clearing(id).usedBuildingSlots() == 0);
            assert(setupGame.map().clearing(id).keep() == 0);
            assert(setupGame.map().clearing(id).pieces(FactionId::MarquiseDeCat, PieceType::Warrior) == 0);
        }
        expectFailure([&] { setupGame.advancePhase(); });
        expectFailure([&] { setupGame.produceWood(FactionId::MarquiseDeCat); });
        expectFailure([&] { setupGame.placeSetupKeep(FactionId::EyrieDynasties, 0); });
        expectFailure([&] { setupGame.placeSetupKeep(FactionId::MarquiseDeCat, 4); });
        expectFailure([&] { setupGame.placeSetupCatBuilding(FactionId::MarquiseDeCat, CatBuilding::Sawmill, 0); });
        setupGame.placeSetupKeep(FactionId::MarquiseDeCat, corners[cornerIndex]);
        assert(setupGame.map().clearing(corners[cornerIndex]).keep() == 1);
        expectFailure([&] { setupGame.placeSetupKeep(FactionId::MarquiseDeCat, corners[cornerIndex]); });
        for (int id = 0; id < 12; ++id) {
            assert(setupGame.map().clearing(id).pieces(FactionId::MarquiseDeCat, PieceType::Warrior)
                == (id == opposite[cornerIndex] ? 0 : 1));
        }
        expectFailure([&] { setupGame.placeSetupCatBuilding(FactionId::MarquiseDeCat, CatBuilding::Sawmill, opposite[cornerIndex]); });
        for (std::size_t type = 0; type < placements.size(); ++type) {
            int destination = -1;
            for (int id = 0; id < 12; ++id) {
                const auto options = setupGame.clearingOptions(FactionId::MarquiseDeCat, id);
                if (std::find(options.actions.begin(), options.actions.end(), placements[type]) != options.actions.end()) {
                    destination = id;
                    break;
                }
            }
            assert(destination >= 0);
            setupGame.placeSetupCatBuilding(FactionId::MarquiseDeCat, static_cast<CatBuilding>(type), destination);
            assert(setupGame.catBuildingCount(static_cast<CatBuilding>(type)) == 1);
            expectFailure([&] { setupGame.placeSetupCatBuilding(FactionId::MarquiseDeCat, static_cast<CatBuilding>(type), destination); });
        }
        assert(setupGame.phase() == Phase::Setup && setupGame.activeFaction() == FactionId::EyrieDynasties);
        assert(setupGame.score(FactionId::MarquiseDeCat) == 0 && setupGame.actionsRemaining() == 0);
        assert(setupGame.catWoodCount() == 0);
        expectFailure([&] { setupGame.chooseEyrieLeader(FactionId::EyrieDynasties, "Builder"); });
        expectFailure([&] { setupGame.placeSetupRoost(FactionId::EyrieDynasties, corners[cornerIndex]); });
        setupGame.placeSetupRoost(FactionId::EyrieDynasties, opposite[cornerIndex]);
        assert(setupGame.map().clearing(opposite[cornerIndex]).pieces(FactionId::EyrieDynasties, PieceType::Warrior) == 6);
        assert(setupGame.map().clearing(opposite[cornerIndex]).pieces(FactionId::EyrieDynasties, PieceType::Building) == 1);
        expectFailure([&] { setupGame.chooseEyrieLeader(FactionId::EyrieDynasties, "Unknown"); });
        assert(setupGame.phase() == Phase::Setup);
        setupGame.chooseEyrieLeader(FactionId::EyrieDynasties, "Builder");
        assert(setupGame.phase() == Phase::Birdsong && setupGame.activeFaction() == FactionId::EyrieDynasties);
        assert(setupGame.hand(FactionId::EyrieDynasties).size() == 3);
        assert(setupGame.eyrieState().decree[0].size() == 1);
        assert(setupGame.eyrieState().decree[1].size() == 1);
        expectFailure([&] { setupGame.placeSetupRoost(FactionId::EyrieDynasties, opposite[cornerIndex]); });
        setupGame.reset();
        assert(setupGame.phase() == Phase::Setup && !setupGame.setupState().keep);
    }

    SingleGame noCatsSetup({FactionId::WoodlandAlliance, FactionId::EyrieDynasties}, 32);
    assert(noCatsSetup.setupState().step == "place_roost");
    assert(noCatsSetup.officers(FactionId::WoodlandAlliance) == 0);
    for (const int corner : corners) {
        assert(noCatsSetup.clearingOptions(FactionId::EyrieDynasties, corner).actions == std::vector<std::string>{"place_roost"});
    }
    noCatsSetup.placeSetupRoost(FactionId::EyrieDynasties, 2);
    noCatsSetup.chooseEyrieLeader(FactionId::EyrieDynasties, "Despot");
    assert(noCatsSetup.activeFaction() == FactionId::WoodlandAlliance && noCatsSetup.phase() == Phase::Birdsong);

    SingleGame noEyrieSetup({FactionId::WoodlandAlliance, FactionId::MarquiseDeCat}, 33);
    noEyrieSetup.placeSetupKeep(FactionId::MarquiseDeCat, 0);
    noEyrieSetup.placeSetupCatBuilding(FactionId::MarquiseDeCat, CatBuilding::Sawmill, 0);
    expectFailure([&] { noEyrieSetup.placeSetupCatBuilding(FactionId::MarquiseDeCat, CatBuilding::Workshop, 0); });
    noEyrieSetup.placeSetupCatBuilding(FactionId::MarquiseDeCat, CatBuilding::Workshop, 1);
    noEyrieSetup.placeSetupCatBuilding(FactionId::MarquiseDeCat, CatBuilding::Recruiter, 3);
    assert(noEyrieSetup.phase() == Phase::Birdsong && noEyrieSetup.activeFaction() == FactionId::WoodlandAlliance);

    SingleGame game({FactionId::MarquiseDeCat, FactionId::EyrieDynasties}, 7, SetupMode::Preset);
    assert(game.hand(FactionId::MarquiseDeCat).size() == 3);
    assert(game.hand(FactionId::EyrieDynasties).size() == 3);
    assert(game.deckSize() == 44);
    assert(game.phase() == Phase::Birdsong);
    expectFailure([&] { game.move(FactionId::MarquiseDeCat, 0, 1, 1); });
    expectFailure([&] { game.advancePhase(); });
    assert(game.phaseActions(FactionId::MarquiseDeCat) == std::vector<std::string>{"produce_wood"});
    game.produceWood(FactionId::MarquiseDeCat);
    assert(game.catWoodCount() == 1);
    assert(game.map().clearing(0).wood() == 1);
    assert(game.map().clearing(0).keep() == 1);
    expectFailure([&] { game.produceWood(FactionId::MarquiseDeCat); });
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
    assert(game.hand(FactionId::MarquiseDeCat).size() == 3);
    assert(game.canDrawCard(FactionId::MarquiseDeCat));
    game.drawCard(FactionId::MarquiseDeCat);
    assert(game.hand(FactionId::MarquiseDeCat).size() == 4);
    assert(game.deckSize() == 43);
    expectFailure([&] { game.drawCard(FactionId::MarquiseDeCat); });

    game.awardPoints(FactionId::MarquiseDeCat, 30);
    assert(game.winner() == FactionId::MarquiseDeCat);
    expectFailure([&] { game.advancePhase(); });

    SingleGame allianceGame({FactionId::WoodlandAlliance, FactionId::MarquiseDeCat}, 11, SetupMode::Preset);
    assert(allianceGame.supporters(FactionId::WoodlandAlliance).size() == 3);
    assert(allianceGame.officers(FactionId::WoodlandAlliance) == 0);
    const Suit supporterSuit = allianceGame.supporters(FactionId::WoodlandAlliance).front().getCardSuit();
    int sympathyClearing = 1;
    if (supporterSuit != Suit::Bird) {
        while (allianceGame.map().clearing(sympathyClearing).suit() != supporterSuit) {
            ++sympathyClearing;
        }
    }
    allianceGame.spreadSympathy(FactionId::WoodlandAlliance, sympathyClearing);
    assert(allianceGame.map().clearing(sympathyClearing).pieces(
        FactionId::WoodlandAlliance, PieceType::Token) == 1);
    allianceGame.advancePhase();
    const auto supporterCount = allianceGame.supporters(FactionId::WoodlandAlliance).size();
    allianceGame.mobilize(FactionId::WoodlandAlliance, 0);
    assert(allianceGame.supporters(FactionId::WoodlandAlliance).size() == supporterCount + 1);
    allianceGame.advancePhase();
    assert(allianceGame.actionsRemaining() == 0);

    SingleGame guerrillaGame({FactionId::MarquiseDeCat, FactionId::WoodlandAlliance}, 12, SetupMode::Preset);
    guerrillaGame.produceWood(FactionId::MarquiseDeCat);
    guerrillaGame.advancePhase();
    guerrillaGame.placePiece(FactionId::WoodlandAlliance, PieceType::Warrior, 0, 2);
    const auto guerrillaBattle = guerrillaGame.battle(
        FactionId::MarquiseDeCat, FactionId::WoodlandAlliance, 0, 3, 1);
    assert(guerrillaBattle.hitsToDefender == 1);
    assert(guerrillaBattle.hitsToAttacker == 2);

    SingleGame catGame({FactionId::MarquiseDeCat, FactionId::EyrieDynasties}, 21, SetupMode::Preset);
    catGame.produceWood(FactionId::MarquiseDeCat);
    catGame.advancePhase();
    assert(catGame.catBuildingCount(CatBuilding::Sawmill) == 1);
    assert(catGame.catBuildingCount(CatBuilding::Workshop) == 1);
    assert(catGame.catBuildingCount(CatBuilding::Recruiter) == 1);
    expectFailure([&] { catGame.buildCatBuilding(FactionId::MarquiseDeCat, CatBuilding::Sawmill, 0); });
    assert(catGame.catWoodCount() == 1 && catGame.actionsRemaining() == 3);
    catGame.buildCatBuilding(FactionId::MarquiseDeCat, CatBuilding::Recruiter, 1);
    assert(catGame.catWoodCount() == 0 && catGame.actionsRemaining() == 2);
    assert(catGame.score(FactionId::MarquiseDeCat) == 1);
    assert(catGame.catBuildCost(CatBuilding::Recruiter) == 2);
    catGame.recruit(FactionId::MarquiseDeCat, 0);
    assert(catGame.map().clearing(1).pieces(FactionId::MarquiseDeCat, PieceType::Warrior) == 2);
    assert(catGame.map().clearing(3).pieces(FactionId::MarquiseDeCat, PieceType::Warrior) == 2);
    expectFailure([&] { catGame.recruit(FactionId::MarquiseDeCat, 0); });
    catGame.move(FactionId::MarquiseDeCat, 1, 0, 1);
    assert(catGame.actionsRemaining() == 0 && catGame.catState().marchMovesRemaining == 1);
    expectFailure([&] { catGame.move(FactionId::MarquiseDeCat, 0, 9, 1); });
    assert(catGame.catState().marchMovesRemaining == 1);
    catGame.move(FactionId::MarquiseDeCat, 0, 3, 1);
    assert(catGame.actionsRemaining() == 0 && catGame.catState().marchMovesRemaining == 0);
    expectFailure([&] { catGame.move(FactionId::MarquiseDeCat, 3, 0, 1); });

    SingleGame multiWarriorMarch({FactionId::MarquiseDeCat, FactionId::EyrieDynasties}, 34, SetupMode::Preset);
    multiWarriorMarch.produceWood(FactionId::MarquiseDeCat);
    multiWarriorMarch.advancePhase();
    multiWarriorMarch.placePiece(FactionId::MarquiseDeCat, PieceType::Warrior, 0, 4);
    const int warriorsAtZero = multiWarriorMarch.map().clearing(0).pieces(
        FactionId::MarquiseDeCat, PieceType::Warrior);
    const int warriorsAtOne = multiWarriorMarch.map().clearing(1).pieces(
        FactionId::MarquiseDeCat, PieceType::Warrior);
    const int warriorsAtTwo = multiWarriorMarch.map().clearing(2).pieces(
        FactionId::MarquiseDeCat, PieceType::Warrior);
    multiWarriorMarch.move(FactionId::MarquiseDeCat, 0, 1, 3);
    assert(multiWarriorMarch.map().clearing(0).pieces(
        FactionId::MarquiseDeCat, PieceType::Warrior) == warriorsAtZero - 3);
    assert(multiWarriorMarch.map().clearing(1).pieces(
        FactionId::MarquiseDeCat, PieceType::Warrior) == warriorsAtOne + 3);
    assert(multiWarriorMarch.actionsRemaining() == 2);
    assert(multiWarriorMarch.catState().marchMovesRemaining == 1);
    multiWarriorMarch.move(FactionId::MarquiseDeCat, 1, 2, 2);
    assert(multiWarriorMarch.map().clearing(1).pieces(
        FactionId::MarquiseDeCat, PieceType::Warrior) == warriorsAtOne + 1);
    assert(multiWarriorMarch.map().clearing(2).pieces(
        FactionId::MarquiseDeCat, PieceType::Warrior) == warriorsAtTwo + 2);
    assert(multiWarriorMarch.actionsRemaining() == 2);
    assert(multiWarriorMarch.catState().marchMovesRemaining == 0);

    SingleGame disconnectedGame({FactionId::MarquiseDeCat, FactionId::EyrieDynasties}, 22, SetupMode::Preset);
    disconnectedGame.produceWood(FactionId::MarquiseDeCat);
    disconnectedGame.advancePhase();
    disconnectedGame.placePiece(FactionId::EyrieDynasties, PieceType::Warrior, 1, 5);
    disconnectedGame.placePiece(FactionId::EyrieDynasties, PieceType::Warrior, 3, 5);
    disconnectedGame.placePiece(FactionId::EyrieDynasties, PieceType::Warrior, 4, 5);
    expectFailure([&] { disconnectedGame.buildCatBuilding(FactionId::MarquiseDeCat, CatBuilding::Workshop, 8); });
    assert(disconnectedGame.catWoodCount() == 1 && disconnectedGame.actionsRemaining() == 3);

    SingleGame overworkGame({FactionId::MarquiseDeCat, FactionId::EyrieDynasties}, 23, SetupMode::Preset);
    const auto overworkSuit = overworkGame.hand(FactionId::MarquiseDeCat).front().getCardSuit();
    int overworkClearing = 0;
    if (overworkSuit != Suit::Bird && overworkSuit != overworkGame.map().clearing(0).suit()) {
        while (overworkGame.map().clearing(overworkClearing).suit() != overworkSuit) ++overworkClearing;
        overworkGame.map().clearing(overworkClearing).addCatBuilding(CatBuilding::Sawmill);
    }
    overworkGame.produceWood(FactionId::MarquiseDeCat);
    overworkGame.advancePhase();
    const int woodBeforeOverwork = overworkGame.catWoodCount();
    overworkGame.overwork(FactionId::MarquiseDeCat, overworkClearing, 0);
    assert(overworkGame.catWoodCount() == woodBeforeOverwork + 1);
    assert(overworkGame.hand(FactionId::MarquiseDeCat).size() == 2 && overworkGame.actionsRemaining() == 2);

    bool checkedBirdAction = false;
    bool checkedItemCraft = false;
    for (unsigned int seed = 1; seed <= 100 && (!checkedBirdAction || !checkedItemCraft); ++seed) {
        SingleGame candidate({FactionId::MarquiseDeCat, FactionId::Vagabond}, seed, SetupMode::Preset);
        candidate.produceWood(FactionId::MarquiseDeCat);
        candidate.advancePhase();
        for (std::size_t index = 0; index < candidate.hand(FactionId::MarquiseDeCat).size(); ++index) {
            const auto options = candidate.cardActions(FactionId::MarquiseDeCat, index);
            if (!checkedBirdAction && candidate.hand(FactionId::MarquiseDeCat)[index].getCardSuit() == Suit::Bird) {
                auto birdGame = candidate;
                birdGame.recruit(FactionId::MarquiseDeCat, 0);
                birdGame.move(FactionId::MarquiseDeCat, 0, 1, 1);
                birdGame.finishMarch(FactionId::MarquiseDeCat);
                birdGame.move(FactionId::MarquiseDeCat, 1, 0, 1);
                birdGame.finishMarch(FactionId::MarquiseDeCat);
                assert(birdGame.actionsRemaining() == 0);
                birdGame.spendBirdCard(FactionId::MarquiseDeCat, index);
                assert(birdGame.actionsRemaining() == 1);
                checkedBirdAction = true;
            }
            if (!checkedItemCraft && std::find(options.begin(), options.end(), "craft") != options.end()) {
                auto craftGame = candidate;
                auto lateCraftGame = candidate;
                lateCraftGame.move(FactionId::MarquiseDeCat, 0, 1, 1);
                expectFailure([&] { lateCraftGame.craft(FactionId::MarquiseDeCat, index); });
                assert(lateCraftGame.hand(FactionId::MarquiseDeCat).size() == 3);
                craftGame.craft(FactionId::MarquiseDeCat, index);
                assert(craftGame.hand(FactionId::MarquiseDeCat).size() == 2);
                assert(craftGame.actionsRemaining() == 3);
                assert(craftGame.score(FactionId::MarquiseDeCat) > 0);
                assert(craftGame.craftedItems(FactionId::MarquiseDeCat).size() == 1);
                checkedItemCraft = true;
            }
        }
    }
    assert(checkedBirdAction && checkedItemCraft);

    SingleGame handLimitGame({FactionId::MarquiseDeCat, FactionId::Vagabond}, 24, SetupMode::Preset);
    for (const int clearing : {2, 5, 6, 8}) handLimitGame.map().clearing(clearing).addCatBuilding(CatBuilding::Recruiter);
    handLimitGame.produceWood(FactionId::MarquiseDeCat);
    handLimitGame.advancePhase();
    handLimitGame.advancePhase();
    handLimitGame.drawCard(FactionId::MarquiseDeCat);
    assert(handLimitGame.hand(FactionId::MarquiseDeCat).size() == 6);
    expectFailure([&] { handLimitGame.advancePhase(); });
    handLimitGame.discardCard(FactionId::MarquiseDeCat, 0);
    expectFailure([&] { handLimitGame.discardCard(FactionId::MarquiseDeCat, 0); });
    handLimitGame.advancePhase();
    handLimitGame.advancePhase();
    handLimitGame.advancePhase();
    handLimitGame.drawCard(FactionId::Vagabond);
    handLimitGame.advancePhase();
    assert(!handLimitGame.catState().woodProduced);
    assert(handLimitGame.phaseActions(FactionId::MarquiseDeCat) == std::vector<std::string>{"produce_wood"});

    auto vagabondGame = [] {
        for (unsigned seed = 1; seed < 100; ++seed) {
            SingleGame game({FactionId::Vagabond, FactionId::MarquiseDeCat}, seed, SetupMode::Preset);
            auto preview = game;
            preview.advancePhase();
            if (preview.explore(FactionId::Vagabond, 4) == "hammer") return game;
        }
        throw std::logic_error("No hammer fixture seed found");
    }();
    assert(vagabondGame.map().clearing(4).pieces(FactionId::Vagabond, PieceType::Warrior) == 1);
    vagabondGame.advancePhase();
    const auto foundItem = vagabondGame.explore(FactionId::Vagabond, 4);
    assert(foundItem == "hammer");
    assert(!vagabondGame.map().clearing(4).hasRuin());
    assert(vagabondGame.score(FactionId::Vagabond) == 1);
    assert(vagabondGame.vagabondItems().hammers == 1);
    assert(vagabondGame.vagabondItems().exhaustedTorches == 1);
    vagabondGame.move(FactionId::Vagabond, 4, 2, 1);
    assert(vagabondGame.vagabondItems().exhaustedBoots == 1);
    vagabondGame.battle(FactionId::Vagabond, FactionId::MarquiseDeCat, 2, 1, 1);
    assert(vagabondGame.vagabondItems().damaged == 1);
    assert(vagabondGame.relationship(FactionId::MarquiseDeCat) == -1);
    vagabondGame.repair(FactionId::Vagabond);
    assert(vagabondGame.vagabondItems().damaged == 0);
    assert(vagabondGame.vagabondItems().exhaustedHammers == 1);

    SingleGame vagabondOutrageGame({FactionId::Vagabond, FactionId::WoodlandAlliance}, 14, SetupMode::Preset);
    vagabondOutrageGame.placePiece(FactionId::WoodlandAlliance, PieceType::Token, 4);
    const auto allianceSupportersBefore = vagabondOutrageGame.supporters(
        FactionId::WoodlandAlliance).size();
    vagabondOutrageGame.advancePhase();
    vagabondOutrageGame.battle(
        FactionId::Vagabond, FactionId::WoodlandAlliance, 4, 3, 0);
    assert(vagabondOutrageGame.supporters(FactionId::WoodlandAlliance).size()
        == allianceSupportersBefore + 1);

    SingleGame eyrieGame({FactionId::EyrieDynasties, FactionId::MarquiseDeCat}, 15, SetupMode::Preset);
    expectFailure([&] { eyrieGame.advancePhase(); });
    eyrieGame.chooseEyrieLeader(FactionId::EyrieDynasties, "Commander");
    assert(eyrieGame.eyrieState().leader == "Commander");
    assert(eyrieGame.eyrieState().decree[1].size() == 1);
    assert(eyrieGame.eyrieState().decree[2].size() == 1);
    eyrieGame.addToDecree(FactionId::EyrieDynasties, 0, "Recruit");
    assert(eyrieGame.hand(FactionId::EyrieDynasties).size() == 2);
    assert(eyrieGame.eyrieState().cardsAddedThisBirdsong == 1);
    eyrieGame.advancePhase();
    assert(eyrieGame.actionsRemaining() == 3);

    SingleGame decreeGame({FactionId::EyrieDynasties, FactionId::MarquiseDeCat}, 18, SetupMode::Preset);
    const auto buildSuit = decreeGame.hand(FactionId::EyrieDynasties).front().getCardSuit();
    decreeGame.chooseEyrieLeader(FactionId::EyrieDynasties, "Builder");
    decreeGame.addToDecree(FactionId::EyrieDynasties, 0, "Build");
    decreeGame.advancePhase();
    assert(!decreeGame.canAdvancePhase(FactionId::EyrieDynasties));
    expectFailure([&] { decreeGame.move(FactionId::EyrieDynasties, 9, 6, 1); });
    assert(decreeGame.actionsRemaining() == 3);
    assert(decreeGame.clearingOptions(FactionId::EyrieDynasties, 9).actions == std::vector<std::string>{"recruit"});
    decreeGame.recruit(FactionId::EyrieDynasties, 9);
    assert(decreeGame.currentDecreeColumn() == "Move");
    const auto optionsBefore = decreeGame.clearingOptions(FactionId::EyrieDynasties, 9);
    assert(!optionsBefore.moveTargets.empty());
    assert(decreeGame.actionsRemaining() == 2);
    decreeGame.move(FactionId::EyrieDynasties, 9, 6, 3);
    assert(decreeGame.currentDecreeColumn() == "Build");
    int buildClearing = 1;
    while ((buildSuit != Suit::Bird && decreeGame.map().clearing(buildClearing).suit() != buildSuit)
        || decreeGame.map().clearing(buildClearing).usedBuildingSlots()
            + (decreeGame.map().clearing(buildClearing).hasRuin() ? 1 : 0)
            >= decreeGame.map().clearing(buildClearing).buildingSlots()) ++buildClearing;
    decreeGame.placePiece(FactionId::EyrieDynasties, PieceType::Warrior, buildClearing, 5);
    decreeGame.buildRoost(FactionId::EyrieDynasties, buildClearing);
    assert(decreeGame.actionsRemaining() == 0);
    decreeGame.advancePhase();
    assert(decreeGame.score(FactionId::EyrieDynasties) == 1);

    SingleGame turmoilGame({FactionId::EyrieDynasties, FactionId::MarquiseDeCat}, 19, SetupMode::Preset);
    turmoilGame.chooseEyrieLeader(FactionId::EyrieDynasties, "Commander");
    turmoilGame.addToDecree(FactionId::EyrieDynasties, 0, "Move");
    turmoilGame.advancePhase();
    turmoilGame.removePiece(FactionId::EyrieDynasties, PieceType::Warrior, 9, 6);
    turmoilGame.awardPoints(FactionId::EyrieDynasties, 8);
    assert(turmoilGame.canAdvancePhase(FactionId::EyrieDynasties));
    turmoilGame.advancePhase();
    assert(turmoilGame.eyrieState().needsLeaderAfterTurmoil);
    assert(!turmoilGame.canDrawCard(FactionId::EyrieDynasties));
    expectFailure([&] { turmoilGame.chooseEyrieLeader(FactionId::EyrieDynasties, "Commander"); });
    turmoilGame.chooseEyrieLeader(FactionId::EyrieDynasties, "Despot");
    assert(turmoilGame.canDrawCard(FactionId::EyrieDynasties));
    assert(turmoilGame.score(FactionId::EyrieDynasties) <= 6);

    bool checkedBirdLimit = false;
    bool checkedTwoSuitedCards = false;
    for (unsigned seed = 1; seed <= 200 && (!checkedBirdLimit || !checkedTwoSuitedCards); ++seed) {
        SingleGame cardGame({FactionId::EyrieDynasties, FactionId::MarquiseDeCat}, seed, SetupMode::Preset);
        std::vector<std::size_t> birds;
        std::vector<std::size_t> suited;
        const auto& cards = cardGame.hand(FactionId::EyrieDynasties);
        for (std::size_t index = 0; index < cards.size(); ++index) {
            (cards[index].getCardSuit() == Suit::Bird ? birds : suited).push_back(index);
        }
        cardGame.chooseEyrieLeader(FactionId::EyrieDynasties, "Despot");
        if (birds.size() >= 2 && !checkedBirdLimit) {
            cardGame.addToDecree(FactionId::EyrieDynasties, birds[0], "Move");
            expectFailure([&] { cardGame.addToDecree(FactionId::EyrieDynasties, birds[1] - 1, "Move"); });
            assert(cardGame.eyrieState().cardsAddedThisBirdsong == 1);
            assert(cardGame.hand(FactionId::EyrieDynasties).size() == 2);
            checkedBirdLimit = true;
        } else if (suited.size() >= 2 && !checkedTwoSuitedCards) {
            cardGame.addToDecree(FactionId::EyrieDynasties, suited[0], "Move");
            cardGame.addToDecree(FactionId::EyrieDynasties, suited[1] - 1, "Build");
            cardGame.advancePhase();
            assert(cardGame.actionsRemaining() == 4);
            checkedTwoSuitedCards = true;
        }
    }
    assert(checkedBirdLimit && checkedTwoSuitedCards);

    std::cout << "All Root core logic tests passed.\n";
}
