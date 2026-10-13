#include "game.hpp"

#include <algorithm>
#include <stdexcept>

namespace {
bool matches(const ShareCard& card, Suit suit) {
    return card.getCardSuit() == suit || card.getCardSuit() == Suit::Bird;
}
}

void SingleGame::validateBattle(FactionId attacker, FactionId defender, int clearing) const {
    requireAction(attacker);
    player(defender);
    if (attacker == defender) throw std::logic_error("Cannot battle your own faction");
    const auto& land = map_.clearing(clearing);
    if (land.pieces(attacker, PieceType::Warrior) == 0
        || land.pieces(defender, PieceType::Warrior) + land.pieces(defender, PieceType::Building)
            + land.pieces(defender, PieceType::Token) == 0) {
        throw std::logic_error("Both factions need pieces in the clearing");
    }
    if (attacker == FactionId::Vagabond && readyItems("sword") == 0) {
        throw std::logic_error("Battle needs a ready sword");
    }
    if (attacker == FactionId::EyrieDynasties && !effectAction_) requireDecree("Battle", land.suit());
}

void SingleGame::beginBattle(FactionId attacker, FactionId defender, int clearing, int first, int second) {
    validateBattle(attacker, defender, clearing);
    if (first < -1 || first > 3 || second < -1 || second > 3 || (first == -1) != (second == -1)) {
        throw std::invalid_argument("Root dice range from zero to three");
    }
    if (attacker == FactionId::EyrieDynasties && !effectAction_) {
        resolveDecree("Battle", requireDecree("Battle", map_.clearing(clearing).suit()));
    }
    if (attacker == FactionId::Vagabond) exhaustItem("sword");
    else if (!effectAction_) consumeAction();
    BattleState state;
    state.id = ++battleId_;
    state.attacker = attacker;
    state.defender = defender;
    state.clearing = clearing;
    state.result.attackerRoll = first;
    state.result.defenderRoll = second;
    battle_ = state;
    player(attacker).phaseStarted = true;
    completedBattle_.reset();
    if (attacker == FactionId::Vagabond) {
        std::vector<RuleChoice> choices{{"pass", "Battle alone"}};
        for (const auto& other : players_) {
            const auto faction = other.faction.id();
            if (faction != attacker && faction != defender && relationship(faction) == 3
                && map_.clearing(clearing).pieces(faction, PieceType::Warrior) > 0) {
                choices.push_back({std::to_string(static_cast<int>(faction)), std::string(factionName(faction))});
            }
        }
        if (choices.size() > 1) decisions_.emplace_back(attacker, "allied_battle", "Battle with an Ally?", choices);
    }
    progressBattle();
}

BattleResult SingleGame::battle(FactionId attacker, FactionId defender, int clearing) {
    beginBattle(attacker, defender, clearing);
    while (!decisions_.empty()) {
        const auto decision = decisions_.front();
        const auto pass = std::find_if(decision.choices.begin(), decision.choices.end(),
                                      [](const auto& choice) { return choice.value == "pass"; });
        respond(decision.faction, pass == decision.choices.end() ? decision.choices.front().value : "pass");
    }
    return completedBattle_->result;
}

BattleResult SingleGame::battle(FactionId attacker, FactionId defender, int clearing, int first, int second) {
    beginBattle(attacker, defender, clearing, first, second);
    while (!decisions_.empty()) {
        const auto decision = decisions_.front();
        const auto pass = std::find_if(decision.choices.begin(), decision.choices.end(),
                                      [](const auto& choice) { return choice.value == "pass"; });
        respond(decision.faction, pass == decision.choices.end() ? decision.choices.front().value : "pass");
    }
    return completedBattle_->result;
}

void SingleGame::progressBattle() {
    if (winner_) {
        if (battle_) completedBattle_ = battle_;
        battle_.reset();
        decisions_.clear();
        return;
    }
    while (battle_ && decisions_.empty()) {
        auto& battle = *battle_;
        const auto& land = map_.clearing(battle.clearing);
        const int stage = battle.stage++;
        if (stage < 2) {
            if ((stage == 0 && hasEffect(battle.attacker, "Scouting Party"))
                || (stage == 1 && !battle.ambushed)) continue;
            const auto actor = stage == 0 ? battle.defender : battle.attacker;
            std::vector<RuleChoice> choices{{"pass", "Pass"}};
            const auto& cards = hand(actor);
            for (std::size_t index = 0; index < cards.size(); ++index) {
                if (cards[index].getEffectType() == ShareCard::CardEffectType::Ambush && matches(cards[index], land.suit())) {
                    choices.push_back({std::to_string(index), cards[index].getCardName()});
                }
            }
            // Ask even with no matching card, so the request does not reveal the defender's hand.
            decisions_.emplace_back(actor, stage == 0 ? "ambush" : "foil",
                                    stage == 0 ? "Ambush?" : "Counter the Ambush?", choices);
        } else if (stage == 2) {
            if (battle.ambushed) {
                battle.result.ambushHits = 2;
                queueLosses(battle.attacker, battle.clearing, 2, battle.defender);
            }
        } else if (stage == 3 || stage == 4) {
            if (land.pieces(battle.attacker, PieceType::Warrior) == 0) {
                battle.stage = 6;
                const int ambush = battle.result.ambushHits;
                battle.result = {};
                battle.result.ambushHits = ambush;
                continue;
            }
            const auto actor = stage == 3 ? battle.attacker : battle.defender;
            battle.result.diceRolled = true;
            if (battle.result.attackerRoll < 0) {
                std::uniform_int_distribution<int> die(0, 3);
                const int first = die(random_);
                const int second = die(random_);
                battle.result.attackerRoll = battle.defender == FactionId::WoodlandAlliance ? std::min(first, second) : std::max(first, second);
                battle.result.defenderRoll = battle.defender == FactionId::WoodlandAlliance ? std::max(first, second) : std::min(first, second);
            }
            std::vector<RuleChoice> choices{{"pass", "Continue"}};
            const auto& cards = persistentCards(actor);
            for (std::size_t index = 0; index < cards.size(); ++index) {
                const auto& name = cards[index].getCardName();
                if (name == "Armorers" || (actor == battle.attacker && name == "Brutal Tactics" && battle.attackerExtra == 0)
                    || (actor == battle.defender && name == "Sappers")) {
                    choices.push_back({std::to_string(index), name});
                }
            }
            if (choices.size() > 1) decisions_.emplace_back(actor, "battle_effect", "Battle effects", choices);
        } else if (stage == 5) {
            if (battle.result.attackerRoll < 0) {
                std::uniform_int_distribution<int> die(0, 3);
                battle.result.attackerRoll = die(random_);
                battle.result.defenderRoll = die(random_);
            }
            const int high = std::max(battle.result.attackerRoll, battle.result.defenderRoll);
            const int low = std::min(battle.result.attackerRoll, battle.result.defenderRoll);
            battle.result.attackerRoll = battle.defender == FactionId::WoodlandAlliance ? low : high;
            battle.result.defenderRoll = battle.defender == FactionId::WoodlandAlliance ? high : low;
            const int attackerCapacity = battle.attacker == FactionId::Vagabond ? undamagedItems("sword")
                + (battle.ally == FactionId::Count ? 0 : land.pieces(battle.ally, PieceType::Warrior))
                : land.pieces(battle.attacker, PieceType::Warrior);
            const int defenderCapacity = battle.defender == FactionId::Vagabond ? undamagedItems("sword")
                : land.pieces(battle.defender, PieceType::Warrior);
            battle.result.hitsToDefender = (battle.defenderArmor ? 0 : std::min(battle.result.attackerRoll, attackerCapacity))
                + battle.attackerExtra + (defenderCapacity == 0 ? 1 : 0)
                + (battle.attacker == FactionId::EyrieDynasties && eyrieState().leader == "Commander" ? 1 : 0);
            battle.result.hitsToAttacker = (battle.attackerArmor ? 0 : std::min(battle.result.defenderRoll, defenderCapacity))
                + battle.defenderExtra;
            queueLosses(battle.defender, battle.clearing, battle.result.hitsToDefender, battle.attacker);
            queueLosses(battle.attacker, battle.clearing, battle.result.hitsToAttacker, battle.defender);
        } else {
            if (battle.ally != FactionId::Count && battle.alliedLosses > battle.damagedItems) {
                player(FactionId::Vagabond).relationships.at(factionIndex(battle.ally)) = -1;
            }
            if (battle.attacker == FactionId::EyrieDynasties && eyrieState().leader == "Despot"
                && battle.result.defenderCardboardRemoved > 0) awardPoints(battle.attacker, 1);
            completedBattle_ = battle;
            battle_.reset();
        }
    }
    if (winner_) {
        if (battle_) completedBattle_ = battle_;
        battle_.reset();
        decisions_.clear();
    }
}

std::vector<RuleChoice> SingleGame::lossChoices(FactionId faction, int clearing) const {
    std::vector<RuleChoice> choices;
    if (faction == FactionId::Vagabond) {
        for (const auto& item : inventory_) if (!item.damaged) {
            choices.push_back({std::to_string(item.id), item.type + " #" + std::to_string(item.id)
                + (item.exhausted ? " (exhausted)" : " (ready)")});
        }
        if (battle_ && battle_->attacker == faction && battle_->ally != FactionId::Count
            && map_.clearing(clearing).pieces(battle_->ally, PieceType::Warrior) > 0) {
            choices.push_back({"ally", std::string(factionName(battle_->ally)) + " warrior"});
        }
        return choices;
    }
    const auto& land = map_.clearing(clearing);
    if (land.pieces(faction, PieceType::Warrior) > 0) return {{"warrior", "Warrior"}};
    if (faction == FactionId::MarquiseDeCat) {
        for (const auto type : {CatBuilding::Sawmill, CatBuilding::Workshop, CatBuilding::Recruiter}) {
            const std::string name = type == CatBuilding::Sawmill ? "sawmill" : type == CatBuilding::Workshop ? "workshop" : "recruiter";
            if (land.catBuildings(type)) choices.push_back({name, name});
        }
        const int typed = land.catBuildings(CatBuilding::Sawmill) + land.catBuildings(CatBuilding::Workshop) + land.catBuildings(CatBuilding::Recruiter);
        if (land.pieces(faction, PieceType::Building) > typed) choices.push_back({"building", "Building"});
        if (land.keep()) choices.push_back({"keep", "Keep"});
        if (land.wood()) choices.push_back({"wood", "Wood"});
    } else {
        if (land.pieces(faction, PieceType::Building)) choices.push_back({"building", faction == FactionId::EyrieDynasties ? "Roost" : "Base"});
        if (land.pieces(faction, PieceType::Token)) choices.push_back({"token", "Sympathy"});
    }
    return choices;
}

void SingleGame::queueLosses(FactionId faction, int clearing, int hits, FactionId remover) {
    if (hits <= 0) return;
    int hospitalized = 0;
    if (faction != FactionId::Vagabond) {
        auto& land = map_.clearing(clearing);
        const int warriors = std::min(hits, land.pieces(faction, PieceType::Warrior));
        if (warriors) {
            land.remove(faction, PieceType::Warrior, warriors);
            if (remover == FactionId::Vagabond) {
                auto& relation = player(remover).relationships.at(factionIndex(faction));
                const int infamy = warriors - (relation < 0 ? 0 : 1);
                relation = -1;
                if (activeFaction() == remover && battle_) awardPoints(remover, infamy);
            }
            if (faction == FactionId::MarquiseDeCat) hospitalized = warriors;
        }
        hits -= warriors;
    }
    const auto choices = lossChoices(faction, clearing);
    if (hits > 0 && !choices.empty()) {
        decisions_.emplace_back(faction, "loss", "Choose a loss (" + std::to_string(hits) + " remaining)", choices,
                                clearing, hits, Suit::Any, remover);
    }
    if (hospitalized > 0) hospitals(clearing, hospitalized);
}

void SingleGame::removeChosenPiece(FactionId faction, int clearing, const std::string& piece, FactionId remover) {
    auto& land = map_.clearing(clearing);
    if (piece == "warrior") {
        land.remove(faction, PieceType::Warrior);
        if (remover == FactionId::Vagabond) player(remover).relationships.at(factionIndex(faction)) = -1;
        if (faction == FactionId::MarquiseDeCat) hospitals(clearing, 1);
        return;
    }
    if (piece == "sawmill" || piece == "workshop" || piece == "recruiter") {
        land.removeCatBuilding(piece == "sawmill" ? CatBuilding::Sawmill : piece == "workshop" ? CatBuilding::Workshop : CatBuilding::Recruiter);
    } else if (piece == "wood") land.removeWood();
    else if (piece == "keep") land.removeKeep();
    else if (piece == "building") {
        land.remove(faction, PieceType::Building);
        if (faction == FactionId::WoodlandAlliance) baseLost(land.suit());
    } else {
        land.remove(faction, PieceType::Token);
        if (faction == FactionId::WoodlandAlliance) triggerOutrage(remover, land.suit());
    }
    awardPoints(remover, 1);
    if (battle_) {
        if (faction == battle_->defender) ++battle_->result.defenderCardboardRemoved;
        else ++battle_->result.attackerCardboardRemoved;
        if (remover == FactionId::Vagabond && activeFaction() == remover && relationship(faction) < 0) awardPoints(remover, 1);
    }
}

void SingleGame::hospitals(int clearing, int warriors) {
    if (warriors <= 0) return;
    bool keep = false;
    for (int id = 0; id < 12; ++id) keep = keep || map_.clearing(id).keep() > 0;
    if (!keep) return;
    std::vector<RuleChoice> choices{{"pass", "Return warriors to supply"}};
    const auto& cards = hand(FactionId::MarquiseDeCat);
    for (std::size_t index = 0; index < cards.size(); ++index) if (matches(cards[index], map_.clearing(clearing).suit())) {
        choices.push_back({std::to_string(index), cards[index].getCardName()});
    }
    if (choices.size() > 1) decisions_.emplace_back(FactionId::MarquiseDeCat, "hospitals", "Field Hospitals", choices, clearing, warriors);
}

void SingleGame::respond(FactionId faction, const std::string& choice) {
    if (decisions_.empty() || decisions_.front().faction != faction) throw std::logic_error("This response belongs to another player");
    auto decision = decisions_.front();
    if (std::none_of(decision.choices.begin(), decision.choices.end(), [&](const auto& entry) { return entry.value == choice; })) {
        throw std::logic_error("Invalid response choice");
    }
    decisions_.pop_front();
    if (decision.kind == "allied_battle") {
        if (choice != "pass") battle_->ally = static_cast<FactionId>(std::stoi(choice));
    } else if (decision.kind == "ambush" || decision.kind == "foil") {
        if (choice != "pass") discardHand(player(faction), static_cast<std::size_t>(std::stoi(choice)));
        battle_->ambushed = decision.kind == "ambush" ? choice != "pass" : choice == "pass";
    } else if (decision.kind == "battle_effect") {
        if (choice != "pass") {
            const int index = std::stoi(choice);
            auto& cards = player(faction).persistent;
            const auto name = cards.at(index).getCardName();
            if (name == "Armorers") {
                if (faction == battle_->attacker) battle_->attackerArmor = true;
                else battle_->defenderArmor = true;
            } else if (name == "Sappers") ++battle_->defenderExtra;
            else {
                ++battle_->attackerExtra;
                awardPoints(battle_->defender, 1);
                player(faction).effectsUsed.push_back("Brutal Tactics");
            }
            if (name != "Brutal Tactics") {
                discard(cards.at(index));
                cards.erase(cards.begin() + index);
            }
            // The same battle can use multiple different crafted effects.
            --battle_->stage;
        }
    } else if (decision.kind == "loss") {
        if (faction == FactionId::Vagabond) {
            if (choice == "ally") {
                map_.clearing(decision.clearing).remove(battle_->ally, PieceType::Warrior);
                ++battle_->alliedLosses;
                if (battle_->ally == FactionId::MarquiseDeCat) hospitals(decision.clearing, 1);
            } else {
                itemById(std::stoi(choice)).damaged = true;
                if (battle_) ++battle_->damagedItems;
            }
            syncItems();
        } else removeChosenPiece(faction, decision.clearing, choice, decision.other);
        if (--decision.amount > 0) {
            decision.choices = lossChoices(faction, decision.clearing);
            if (!decision.choices.empty()) decisions_.push_front(std::move(decision));
        }
    } else if (decision.kind == "hospitals") {
        if (choice != "pass") {
            int keep = -1;
            for (int id = 0; id < 12; ++id) if (map_.clearing(id).keep()) keep = id;
            if (keep >= 0) {
                discardHand(player(faction), static_cast<std::size_t>(std::stoi(choice)));
                map_.clearing(keep).add(faction, PieceType::Warrior, decision.amount);
            }
        }
    } else if (decision.kind == "outrage") {
        auto& offender = player(faction);
        const auto index = static_cast<std::size_t>(std::stoi(choice));
        gainSupporter(player(FactionId::WoodlandAlliance), std::move(offender.hand.at(index)));
        offender.hand.erase(offender.hand.begin() + static_cast<std::ptrdiff_t>(index));
    } else if (decision.kind == "supporter_limit") {
        auto& cards = player(faction).supporters;
        const int index = std::stoi(choice);
        discard(cards.at(index));
        cards.erase(cards.begin() + index);
        if (cards.size() > 5) {
            decision.choices.clear();
            for (std::size_t i = 0; i < cards.size(); ++i) decision.choices.push_back({std::to_string(i), cards[i].getCardName()});
            decisions_.push_front(std::move(decision));
        }
    } else if (decision.kind == "reveal") {
        recycleDeck();
        if (!drawPile_.empty()) {
            const auto card = drawPile_.back();
            drawPile_.pop_back();
            recycleDeck();
            gainSupporter(player(FactionId::WoodlandAlliance), card);
        }
    } else if (decision.kind == "hideout_repair") {
        itemById(std::stoi(choice)).damaged = false;
        syncItems();
        decision.choices.clear();
        for (const auto& item : inventory_) if (item.damaged) decision.choices.push_back({std::to_string(item.id), item.type + " #" + std::to_string(item.id)});
        if (--decision.amount > 0 && !decision.choices.empty()) decisions_.push_front(std::move(decision));
        else advancePhase();
    }
    refreshDecisions();
    progressBattle();
}

void SingleGame::refreshDecisions() {
    for (auto found = decisions_.begin(); found != decisions_.end();) {
        auto& decision = *found;
        if (decision.kind == "outrage" || decision.kind == "hospitals") {
            const Suit suit = decision.kind == "outrage" ? decision.suit : map_.clearing(decision.clearing).suit();
            decision.choices.clear();
            if (decision.kind == "hospitals") decision.choices.push_back({"pass", "Return warriors to supply"});
            const auto& cards = hand(decision.faction);
            for (std::size_t index = 0; index < cards.size(); ++index) if (matches(cards[index], suit)) {
                decision.choices.push_back({std::to_string(index), cards[index].getCardName()});
            }
            if (decision.choices.empty()) {
                decision.title = std::string(factionName(decision.faction)) + " reveals: ";
                for (const auto& card : cards) decision.title += card.getCardName() + "; ";
                decision.kind = "reveal";
                decision.faction = FactionId::WoodlandAlliance;
                decision.choices = {{"pass", "Draw a supporter"}};
            }
        } else if (decision.kind == "supporter_limit") {
            const auto& cards = supporters(decision.faction);
            if (cards.size() <= 5) { found = decisions_.erase(found); continue; }
            decision.choices.clear();
            for (std::size_t index = 0; index < cards.size(); ++index) decision.choices.push_back({std::to_string(index), cards[index].getCardName()});
        }
        ++found;
    }
}
