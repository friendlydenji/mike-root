#include <algorithm>
#include <cassert>
#include <iostream>
#include <stdexcept>

#include "card/share_deck.hpp"
#include "game.hpp"

struct GameTestAccess {
    static ShareCard card(const std::string& name) {
        const auto deck = createShareDeck();
        const auto found = std::find_if(deck.begin(), deck.end(), [&](const auto& card) { return card.getCardName() == name; });
        if (found == deck.end()) throw std::logic_error("Unknown fixture card: " + name);
        return *found;
    }
    static void hand(SingleGame& game, FactionId faction, std::initializer_list<const char*> names) {
        auto& hand = game.player(faction).hand;
        hand.clear();
        for (const auto* name : names) hand.push_back(card(name));
    }
    static void supporters(SingleGame& game, std::initializer_list<const char*> names) {
        auto& cards = game.player(FactionId::WoodlandAlliance).supporters;
        cards.clear();
        for (const auto* name : names) cards.push_back(card(name));
    }
    static void phase(SingleGame& game, Phase phase, int actions = 3) {
        game.phase_ = phase;
        game.actionsRemaining_ = actions;
        game.eveningCardDrawn_ = false;
        game.player(game.activeFaction()).phaseStarted = false;
    }
    static void effect(SingleGame& game, FactionId faction, const std::string& name) { game.player(faction).persistent.push_back(card(name)); }
    static void items(SingleGame& game, std::initializer_list<const char*> types) {
        game.inventory_.clear();
        for (const auto* type : types) game.addItem(type);
    }
    static void forest(SingleGame& game, int id) {
        for (int clearing = 0; clearing < 12; ++clearing) {
            auto& land = game.map_.clearing(clearing);
            if (land.pieces(FactionId::Vagabond, PieceType::Warrior)) land.remove(FactionId::Vagabond, PieceType::Warrior);
        }
        game.forest_ = id;
    }
    static void deckRecycle(SingleGame& game) {
        game.drawPile_ = {card("Sword")};
        game.discardPile_ = {card("Crossbow")};
        const auto count = game.hand(FactionId::MarquiseDeCat).size();
        game.drawCards(game.player(FactionId::MarquiseDeCat), 1);
        assert(game.hand(FactionId::MarquiseDeCat).size() == count + 1);
        assert(game.deckSize() == 1 && game.discardSize() == 0);
        assert(game.drawPile_.front().getCardName() == "Crossbow");
    }
    static void aid(SingleGame& game, FactionId target, int card, int item, const std::string& take = "") {
        game.aidChosen(FactionId::Vagabond, target, game.vagabondLocation(), card, item, take);
    }
    static void crafted(SingleGame& game, FactionId faction, const std::string& type) { ++game.player(faction).craftedItems[type]; }
    static void damage(SingleGame& game, int id, bool exhausted = false) {
        auto& item = game.itemById(id); item.damaged = true; item.exhausted = exhausted; game.syncItems();
    }
    static void refresh(SingleGame& game) { game.refreshVagabond(); }
    static void relation(SingleGame& game, FactionId target, int value) { game.player(FactionId::Vagabond).relationships[factionIndex(target)] = value; }
    static void quest(SingleGame& game, int id) { game.quests_ = {{std::to_string(id), "fixture"}}; }
    static void officers(SingleGame& game, int count) { game.player(FactionId::WoodlandAlliance).officers = count; }
    static void exhaust(SingleGame& game, int id) { game.itemById(id).exhausted = true; game.syncItems(); }
};

template <typename Action> void fails(Action action) {
    bool failed = false;
    try { action(); } catch (const std::exception&) { failed = true; }
    assert(failed);
}

void settle(SingleGame& game) {
    int count = 0;
    while (!game.decisions().empty()) {
        assert(++count < 100);
        const auto decision = game.decisions().front();
        const auto pass = std::find_if(decision.choices.begin(), decision.choices.end(), [](const auto& choice) { return choice.value == "pass"; });
        game.respond(decision.faction, pass == decision.choices.end() ? decision.choices.front().value : "pass");
    }
}

void action(SingleGame& game, const std::string& type, const std::string& option = "", int item = -1) {
    const auto rules = game.ruleActions(game.activeFaction());
    const auto found = std::find_if(rules.begin(), rules.end(), [&](const auto& rule) {
        return rule.type == type && (option.empty() || rule.option == option) && (item < 0 || rule.item == item);
    });
    assert(found != rules.end());
    game.executeRuleAction(game.activeFaction(), static_cast<std::size_t>(found - rules.begin()));
}

int main() {
    constexpr auto cats = FactionId::MarquiseDeCat;
    constexpr auto birds = FactionId::EyrieDynasties;
    constexpr auto alliance = FactionId::WoodlandAlliance;
    constexpr auto vagabond = FactionId::Vagabond;
    using Access = GameTestAccess;

    SingleGame deck({cats, vagabond}, 1, SetupMode::Preset);
    assert(deck.deckSize() + deck.hand(cats).size() + deck.hand(vagabond).size() == 50);
    Access::deckRecycle(deck);
    SingleGame cycling({cats, vagabond}, 2, SetupMode::Preset);
    for (int turn = 0; turn < 100; ++turn) {
        const auto faction = cycling.activeFaction();
        if (faction == cats) cycling.produceWood(cats);
        cycling.advancePhase(); cycling.advancePhase(); cycling.drawCard(faction);
        while (cycling.hand(faction).size() > 5) cycling.discardCard(faction, 0);
        assert(cycling.deckSize() + cycling.discardSize() + cycling.hand(cats).size() + cycling.hand(vagabond).size() == 50);
        cycling.advancePhase();
    }
    assert(cycling.deckSize() > 0);

    SingleGame keep({alliance, cats}, 3, SetupMode::Preset);
    Access::supporters(keep, {"Bird Ambush!", "Bird Ambush!", "Bird Ambush!", "Bird Ambush!"});
    fails([&] { keep.spreadSympathy(alliance, 0); });
    assert(keep.supporters(alliance).size() == 4);
    keep.placePiece(alliance, PieceType::Token, 3);
    keep.spreadSympathy(alliance, 8);
    fails([&] { keep.revolt(alliance, 3); });

    SingleGame supply({alliance, cats}, 4, SetupMode::Preset);
    supply.placePiece(alliance, PieceType::Building, 6);
    supply.placePiece(alliance, PieceType::Warrior, 6, 9);
    Access::officers(supply, 1);
    Access::hand(supply, alliance, {"Bird Ambush!"});
    Access::phase(supply, Phase::Daylight);
    fails([&] { supply.train(alliance, 0); });
    Access::phase(supply, Phase::Evening, 1);
    fails([&] { supply.recruit(alliance, 6); });
    supply.drawCard(alliance);
    assert(supply.hand(alliance).size() == 3);
    fails([&] { supply.organize(alliance, 6); });

    SingleGame roost({birds, cats}, 5, SetupMode::Preset);
    roost.chooseEyrieLeader(birds, "Builder");
    roost.addToDecree(birds, 0, "Move");
    roost.removePiece(birds, PieceType::Building, 9);
    fails([&] { roost.advancePhase(); });
    fails([&] { roost.newRoost(birds, 0); });
    roost.newRoost(birds, 2);
    assert(roost.map().clearing(2).pieces(birds, PieceType::Warrior) == 3);
    roost.advancePhase();

    SingleGame ambush({cats, birds}, 6, SetupMode::Preset);
    Access::phase(ambush, Phase::Daylight);
    ambush.placePiece(birds, PieceType::Warrior, 4);
    Access::hand(ambush, birds, {"Bird Ambush!"});
    Access::hand(ambush, cats, {"Bird Ambush!"});
    ambush.beginBattle(cats, birds, 4, 3, 0);
    assert(ambush.decisions().front().kind == "ambush");
    fails([&] { ambush.respond(cats, "pass"); });
    fails([&] { ambush.advancePhase(); });
    ambush.respond(birds, "0");
    assert(ambush.decisions().front().kind == "foil");
    ambush.respond(cats, "0");
    settle(ambush);
    assert(!ambush.pendingBattle());
    assert(ambush.completedBattle()->result.attackerRoll == 3);
    assert(ambush.discardSize() == 2);

    SingleGame stopped({cats, birds}, 7, SetupMode::Preset);
    Access::phase(stopped, Phase::Daylight);
    stopped.placePiece(birds, PieceType::Warrior, 4);
    Access::hand(stopped, birds, {"Bird Ambush!"});
    Access::hand(stopped, cats, {});
    stopped.beginBattle(cats, birds, 4, 3, 3);
    stopped.respond(birds, "0"); stopped.respond(cats, "pass"); settle(stopped);
    assert(stopped.map().clearing(4).pieces(cats, PieceType::Warrior) == 0);
    assert(stopped.map().clearing(4).pieces(birds, PieceType::Warrior) == 1);
    assert(!stopped.completedBattle()->result.diceRolled && stopped.completedBattle()->result.ambushHits == 2);

    SingleGame hospital({birds, cats}, 8, SetupMode::Preset);
    hospital.chooseEyrieLeader(birds, "Commander");
    Access::hand(hospital, birds, {"Bird Ambush!"}); hospital.addToDecree(birds, 0, "Battle");
    Access::phase(hospital, Phase::Daylight);
    hospital.move(birds, 9, 6, 1);
    Access::hand(hospital, cats, {"Bird Ambush!"});
    hospital.beginBattle(birds, cats, 6, 3, 0);
    hospital.respond(cats, "pass");
    assert(hospital.decisions().front().kind == "hospitals");
    const int warriorsAtKeep = hospital.map().clearing(0).pieces(cats, PieceType::Warrior);
    hospital.respond(cats, "0"); settle(hospital);
    assert(hospital.map().clearing(0).pieces(cats, PieceType::Warrior) == warriorsAtKeep + 1);

    SingleGame cardboard({birds, cats}, 9, SetupMode::Preset);
    cardboard.chooseEyrieLeader(birds, "Commander");
    Access::hand(cardboard, birds, {"Bird Ambush!"}); cardboard.addToDecree(birds, 0, "Battle");
    Access::phase(cardboard, Phase::Daylight);
    cardboard.move(birds, 9, 6, 1);
    cardboard.removePiece(cats, PieceType::Warrior, 6);
    cardboard.map().clearing(6).addCatBuilding(CatBuilding::Recruiter);
    cardboard.map().clearing(6).addWood();
    Access::hand(cardboard, cats, {});
    cardboard.beginBattle(birds, cats, 6, 0, 0);
    cardboard.respond(cats, "pass");
    assert(cardboard.decisions().front().kind == "loss");
    cardboard.respond(cats, "recruiter");
    assert(cardboard.map().clearing(6).catBuildings(CatBuilding::Recruiter) == 0);
    settle(cardboard);

    SingleGame effects({cats, birds}, 10, SetupMode::Preset);
    Access::phase(effects, Phase::Daylight);
    effects.placePiece(birds, PieceType::Warrior, 4, 3);
    Access::effect(effects, cats, "Brutal Tactics");
    Access::effect(effects, birds, "Armorers");
    effects.beginBattle(cats, birds, 4, 3, 2);
    effects.respond(birds, "pass"); effects.respond(cats, "0");
    effects.respond(birds, "0"); settle(effects);
    assert(effects.completedBattle()->result.hitsToDefender == 1);
    assert(effects.score(birds) == 1);
    assert(effects.persistentCards(birds).empty());

    SingleGame vag({vagabond, cats}, 11, SetupMode::Preset);
    vag.slip(vagabond, 0);
    fails([&] { vag.slip(vagabond, 1); });
    Access::phase(vag, Phase::Daylight);
    const int boot = vag.inventory().front().id;
    Access::damage(vag, boot);
    Access::damage(vag, vag.inventory()[1].id);
    fails([&] { vag.move(vagabond, 0, 1, 1); });
    Access::items(vag, {"boot", "sword", "hammer", "tea", "bag", "coins"});
    Access::hand(vag, vagabond, {"Bird Ambush!", "Bird Ambush!", "Bird Ambush!"});
    Access::crafted(vag, cats, "crossbow");
    Access::aid(vag, cats, 0, vag.inventory()[0].id, "crossbow");
    Access::aid(vag, cats, 0, vag.inventory()[1].id);
    assert(vag.relationship(cats) == 1 && vag.score(vagabond) == 1);
    Access::aid(vag, cats, 0, vag.inventory()[2].id);
    assert(vag.relationship(cats) == 2 && vag.score(vagabond) == 3);
    assert(vag.craftedItems(cats).at("crossbow") == 0);
    assert(std::any_of(vag.inventory().begin(), vag.inventory().end(), [](const auto& item) { return item.type == "crossbow"; }));

    const int damagedSword = vag.inventory()[1].id;
    Access::damage(vag, damagedSword, true);
    Access::items(vag, {"hammer", "boot", "coins", "bag"});
    const int damagedBoot = vag.inventory()[1].id;
    Access::damage(vag, damagedBoot, true);
    action(vag, "repair", "", damagedBoot);
    assert(!vag.inventory()[1].damaged && vag.inventory()[1].exhausted);
    Access::damage(vag, damagedBoot);
    Access::forest(vag, 12);
    Access::phase(vag, Phase::Evening);
    const auto beforeRest = vag.hand(vagabond).size();
    vag.drawCard(vagabond);
    assert(vag.hand(vagabond).size() == beforeRest + 2);
    assert(!vag.inventory()[1].damaged && !vag.inventory()[1].exhausted);
    assert(vag.inventory()[0].exhausted);

    SingleGame quest({vagabond, cats}, 12, SetupMode::Preset);
    Access::phase(quest, Phase::Daylight);
    quest.move(vagabond, 4, 8, 1);
    Access::items(quest, {"boot", "bag"});
    Access::quest(quest, 2);
    action(quest, "quest", "points");
    assert(quest.score(vagabond) == 1 && quest.inventory()[0].exhausted && quest.inventory()[1].exhausted);

    SingleGame persistent({cats, birds}, 13, SetupMode::Preset);
    Access::phase(persistent, Phase::Daylight);
    persistent.map().clearing(6).addCatBuilding(CatBuilding::Workshop);
    Access::hand(persistent, cats, {"Armorers", "Armorers"});
    persistent.craft(cats, 0);
    assert(persistent.persistentCards(cats).size() == 1);
    fails([&] { persistent.craft(cats, 0); });
    assert(persistent.score(cats) == 0);

    SingleGame favor({cats, birds}, 21, SetupMode::Preset);
    Access::phase(favor, Phase::Daylight);
    for (const int id : {6, 8, 10}) favor.map().clearing(id).addCatBuilding(CatBuilding::Workshop);
    favor.placePiece(birds, PieceType::Warrior, 6, 2);
    favor.placePiece(birds, PieceType::Warrior, 10, 2);
    favor.placePiece(birds, PieceType::Building, 10);
    Access::hand(favor, cats, {"Favor of the Foxes"});
    favor.craft(cats, 0);
    assert(favor.score(cats) == 1 && favor.discardSize() == 1);
    assert(favor.map().clearing(6).pieces(birds, PieceType::Warrior) == 0);
    assert(favor.map().clearing(10).pieces(birds, PieceType::Building) == 0);
    assert(favor.map().clearing(0).keep() == 1);

    SingleGame claim({cats, birds}, 22, SetupMode::Preset);
    Access::phase(claim, Phase::Daylight);
    for (const int id : {3, 6, 10}) claim.map().clearing(id).addCatBuilding(CatBuilding::Workshop);
    Access::hand(claim, cats, {"Royal Claim"});
    claim.craft(cats, 0);
    Access::phase(claim, Phase::Birdsong);
    action(claim, "effect");
    assert(claim.score(cats) == 11 && claim.persistentCards(cats).empty());

    SingleGame bonuses({cats, birds}, 23, SetupMode::Preset);
    Access::effect(bonuses, cats, "Cobbler");
    Access::phase(bonuses, Phase::Evening, 0);
    action(bonuses, "bonus_move");
    assert(bonuses.actionsRemaining() == 0 && bonuses.catState().marchMovesRemaining == 0);
    Access::effect(bonuses, cats, "Command Warren");
    Access::phase(bonuses, Phase::Daylight);
    bonuses.placePiece(birds, PieceType::Warrior, 4);
    action(bonuses, "bonus_battle");
    assert(bonuses.actionsRemaining() == 3);
    settle(bonuses);

    SingleGame refresh({vagabond, cats}, 24, SetupMode::Preset);
    Access::items(refresh, {"tea", "boot", "sword", "hammer", "torch"});
    for (const auto& item : refresh.inventory()) Access::exhaust(refresh, item.id);
    Access::refresh(refresh);
    for (int count = 0; count < 3; ++count) action(refresh, "refresh");
    const auto refreshActions = refresh.ruleActions(vagabond);
    assert(std::none_of(refreshActions.begin(), refreshActions.end(), [](const auto& rule) { return rule.type == "refresh"; }));
    refresh.advancePhase();
    assert(std::count_if(refresh.inventory().begin(), refresh.inventory().end(), [](const auto& item) { return item.exhausted; }) == 2);

    SingleGame capacity({vagabond, cats}, 25, SetupMode::Preset);
    Access::items(capacity, {"boot", "boot", "boot", "boot", "boot", "boot", "boot", "boot", "boot", "bag"});
    Access::phase(capacity, Phase::Evening);
    capacity.drawCard(vagabond);
    fails([&] { capacity.advancePhase(); });
    action(capacity, "discard_item");
    capacity.advancePhase();

    SingleGame allied({vagabond, cats, birds}, 26, SetupMode::Preset);
    Access::phase(allied, Phase::Daylight);
    Access::relation(allied, cats, 3);
    Access::hand(allied, birds, {});
    allied.placePiece(cats, PieceType::Warrior, 4, 2);
    allied.placePiece(birds, PieceType::Warrior, 4, 3);
    allied.beginBattle(vagabond, birds, 4, 3, 2);
    assert(allied.decisions().front().kind == "allied_battle");
    allied.respond(vagabond, std::to_string(static_cast<int>(cats)));
    allied.respond(birds, "pass");
    assert(allied.decisions().front().faction == vagabond);
    allied.respond(vagabond, "ally");
    allied.respond(vagabond, "ally");
    settle(allied);
    assert(allied.completedBattle()->result.hitsToDefender == 3);
    assert(allied.relationship(cats) == -1);

    SingleGame dominance({cats, birds, alliance}, 14, SetupMode::Preset);
    Access::phase(dominance, Phase::Daylight);
    Access::hand(dominance, cats, {"Rabbit Dominance"});
    const auto beforeDominance = dominance.ruleActions(cats);
    assert(std::none_of(beforeDominance.begin(), beforeDominance.end(), [](const auto& rule) { return rule.type == "dominance"; }));
    dominance.awardPoints(cats, 10);
    action(dominance, "dominance");
    assert(dominance.dominance(cats) == Suit::Rabbit);
    dominance.awardPoints(cats, 50);
    assert(!dominance.winner() && dominance.score(cats) == 10);

    SingleGame coalition({vagabond, cats, birds, alliance}, 15, SetupMode::Preset);
    Access::phase(coalition, Phase::Daylight);
    Access::hand(coalition, vagabond, {"Bird Dominance"});
    coalition.awardPoints(vagabond, 10); coalition.awardPoints(birds, 3); coalition.awardPoints(alliance, 2);
    Access::relation(coalition, cats, -1);
    action(coalition, "dominance");
    assert(coalition.coalition() == cats && coalition.relationship(cats) == 0);
    coalition.awardPoints(vagabond, 30);
    assert(!coalition.winner());
    coalition.awardPoints(cats, 30);
    assert(coalition.winner() == cats);

    std::cout << "All audited-rule regression tests passed.\n";
}
