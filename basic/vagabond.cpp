#include "game.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace {
struct Quest {
    const char* name;
    Suit suit;
    const char* first;
    const char* second;
};
const std::array<Quest, 15> questCards{{
    {"fox-fundraising", Suit::Fox, "tea", "coins"}, {"fox-errand", Suit::Fox, "tea", "boot"},
    {"fox-logistics-help", Suit::Fox, "boot", "bag"}, {"fox-repair-a-shed", Suit::Fox, "torch", "hammer"},
    {"fox-give-a-speech", Suit::Fox, "torch", "tea"}, {"rabbit-guard-duty", Suit::Rabbit, "torch", "sword"},
    {"rabbit-errand", Suit::Rabbit, "tea", "boot"}, {"rabbit-give-a-speech", Suit::Rabbit, "torch", "tea"},
    {"rabbit-fend-off-a-bear", Suit::Rabbit, "torch", "crossbow"}, {"rabbit-expel-bandits", Suit::Rabbit, "sword", "sword"},
    {"mouse-expel-bandits", Suit::Mouse, "sword", "sword"}, {"mouse-guard-duty", Suit::Mouse, "torch", "sword"},
    {"mouse-fend-off-a-bear", Suit::Mouse, "torch", "crossbow"}, {"mouse-escort", Suit::Mouse, "boot", "boot"},
    {"mouse-logistics-help", Suit::Mouse, "boot", "bag"}
}};
}

const std::array<std::vector<int>, 7>& SingleGame::forests() {
    static const std::array<std::vector<int>, 7> areas{{
        {0, 1, 2, 4}, {0, 4, 8, 3}, {2, 6, 5, 8, 4}, {3, 8, 7},
        {5, 9, 11, 8}, {5, 6, 9}, {7, 8, 11, 10}
    }};
    return areas;
}

bool SingleGame::vagabondAdjacent(int from, int to) const {
    if (from < 0 || from >= 19 || to < 0 || to >= 19 || from == to) return false;
    if (from < 12 && to < 12) return map_.areAdjacent(from, to);
    if (from < 12) std::swap(from, to);
    const auto& boundary = forests().at(static_cast<std::size_t>(from - 12));
    if (to < 12) return std::find(boundary.begin(), boundary.end(), to) != boundary.end();
    const auto& other = forests().at(static_cast<std::size_t>(to - 12));
    for (std::size_t index = 0; index < boundary.size(); ++index) {
        const int first = boundary[index];
        const int second = boundary[(index + 1) % boundary.size()];
        for (std::size_t j = 0; j < other.size(); ++j) {
            if (other[j] == second && other[(j + 1) % other.size()] == first) return true;
        }
    }
    return false;
}

void SingleGame::initializeVagabond() {
    inventory_.clear();
    nextItemId_ = 0;
    forest_ = -1;
    character_.clear();
    slipped_ = false;
    refreshes_ = 0;
    completedQuests_.fill(0);
    std::array<std::string, 4> items{"boot", "bag", "hammer", "sword"};
    std::shuffle(items.begin(), items.end(), random_);
    ruinItems_.clear();
    const std::array<int, 4> ruins{4, 5, 6, 8};
    for (std::size_t index = 0; index < ruins.size(); ++index) ruinItems_[ruins[index]] = items[index];
    questDeck_.resize(15);
    std::iota(questDeck_.begin(), questDeck_.end(), 0);
    std::shuffle(questDeck_.begin(), questDeck_.end(), random_);
    quests_.clear();
    for (int count = 0; count < 3; ++count) {
        const int quest = questDeck_.back();
        questDeck_.pop_back();
        quests_.push_back({std::to_string(quest), questCards[quest].name});
    }
}

void SingleGame::finishEyrieSetup() {
    if (hasFaction(FactionId::Vagabond)) {
        setup_.faction = FactionId::Vagabond;
        setup_.step = "choose_character";
    } else finishSetup();
}

int SingleGame::vagabondLocation() const {
    if (forest_ >= 12) return forest_;
    for (int id = 0; id < 12; ++id) if (map_.clearing(id).pieces(FactionId::Vagabond, PieceType::Warrior)) return id;
    return -1;
}

void SingleGame::addItem(const std::string& type) {
    inventory_.push_back({nextItemId_++, type, false, false});
    syncItems();
}

void SingleGame::syncItems() {
    if (!hasFaction(FactionId::Vagabond)) return;
    auto& counts = player(FactionId::Vagabond).vagabond;
    counts = {};
    counts.boots = counts.swords = counts.torches = 0;
    for (const auto& item : inventory_) {
        if (item.type == "boot") { ++counts.boots; counts.exhaustedBoots += item.exhausted || item.damaged; }
        if (item.type == "sword") { ++counts.swords; counts.exhaustedSwords += item.exhausted || item.damaged; }
        if (item.type == "torch") { ++counts.torches; counts.exhaustedTorches += item.exhausted || item.damaged; }
        if (item.type == "hammer") { ++counts.hammers; counts.exhaustedHammers += item.exhausted || item.damaged; }
        if (item.type == "tea") { ++counts.tea; counts.exhaustedTea += item.exhausted || item.damaged; }
        counts.damaged += item.damaged;
    }
}

int SingleGame::readyItems(const std::string& type) const {
    return static_cast<int>(std::count_if(inventory_.begin(), inventory_.end(), [&](const auto& item) {
        return item.type == type && !item.exhausted && !item.damaged;
    }));
}

int SingleGame::undamagedItems(const std::string& type) const {
    return static_cast<int>(std::count_if(inventory_.begin(), inventory_.end(), [&](const auto& item) {
        return item.type == type && !item.damaged;
    }));
}

VagabondItem& SingleGame::itemById(int id) {
    const auto found = std::find_if(inventory_.begin(), inventory_.end(), [&](const auto& item) { return item.id == id; });
    if (found == inventory_.end()) throw std::logic_error("Unknown inventory item");
    return *found;
}

void SingleGame::exhaustItem(const std::string& type, int amount) {
    if (readyItems(type) < amount) throw std::logic_error("Not enough ready " + type + " items");
    for (auto& item : inventory_) if (amount > 0 && item.type == type && !item.damaged && !item.exhausted) {
        item.exhausted = true;
        --amount;
    }
    syncItems();
}

bool SingleGame::onTrack(const VagabondItem& item) const {
    if (item.damaged || item.exhausted || (item.type != "bag" && item.type != "tea" && item.type != "coins")) return false;
    int preceding = 0;
    for (const auto& other : inventory_) if (other.id < item.id && other.type == item.type && !other.exhausted && !other.damaged) ++preceding;
    return preceding < 3;
}

int SingleGame::satchelSize() const {
    return static_cast<int>(std::count_if(inventory_.begin(), inventory_.end(), [&](const auto& item) { return !onTrack(item); }));
}
int SingleGame::itemCapacity() const { return 6 + 2 * std::min(3, readyItems("bag")); }
int SingleGame::vagabondItemCount() const { return static_cast<int>(inventory_.size()); }

void SingleGame::refreshVagabond() {
    slipped_ = false;
    refreshes_ = 3 + 2 * std::min(3, readyItems("tea"));
}

void SingleGame::restVagabond() {
    if (forest_ < 12) return;
    for (auto& item : inventory_) if (item.damaged) { item.damaged = false; item.exhausted = false; }
    syncItems();
}

void SingleGame::slip(FactionId faction, int to) {
    requireFactionPhase(faction, Phase::Birdsong);
    if (faction != FactionId::Vagabond || slipped_) throw std::logic_error("Slip is available once per Birdsong");
    if (refreshes_ > 0 && std::any_of(inventory_.begin(), inventory_.end(), [](const auto& item) { return item.exhausted; })) {
        throw std::logic_error("Finish refreshing items before Slip");
    }
    const int from = vagabondLocation();
    if (!vagabondAdjacent(from, to)) throw std::logic_error("Slip to an adjacent clearing or forest");
    if (from < 12) map_.clearing(from).remove(faction, PieceType::Warrior);
    if (to < 12) { forest_ = -1; map_.clearing(to).add(faction, PieceType::Warrior); }
    else forest_ = to;
    slipped_ = true;
    refreshes_ = 0;
    player(faction).phaseStarted = true;
}

std::string SingleGame::explore(FactionId faction, int clearing) {
    requireFactionPhase(faction, Phase::Daylight);
    if (faction != FactionId::Vagabond || vagabondLocation() != clearing || !map_.clearing(clearing).hasRuin()) {
        throw std::logic_error("Explore a ruin in your clearing");
    }
    const auto found = ruinItems_.at(clearing);
    exhaustItem("torch");
    addItem(found);
    ruinItems_.erase(clearing);
    map_.clearing(clearing).removeRuin();
    awardPoints(faction, 1);
    player(faction).phaseStarted = true;
    return found;
}

void SingleGame::aidChosen(FactionId faction, FactionId target, int clearing, int card, int item, const std::string& take) {
    requireFactionPhase(faction, Phase::Daylight);
    if (faction != FactionId::Vagabond || target == faction || target == FactionId::Count || vagabondLocation() != clearing) throw std::logic_error("Invalid Aid target");
    const auto& land = map_.clearing(clearing);
    auto& state = player(faction);
    auto& recipient = player(target);
    if (land.ruleStrength(target) + land.pieces(target, PieceType::Token) == 0
        || (state.hand.at(card).getCardSuit() != land.suit() && state.hand.at(card).getCardSuit() != Suit::Bird)) throw std::logic_error("Aid needs a matching card and faction pieces here");
    auto& exhausted = itemById(item);
    if (exhausted.damaged || exhausted.exhausted) throw std::logic_error("Choose a ready item");
    if (!take.empty() && (!recipient.craftedItems.count(take) || recipient.craftedItems.at(take) == 0)) throw std::logic_error("That crafted item is unavailable");
    exhausted.exhausted = true;
    recipient.hand.push_back(state.hand.at(card));
    state.hand.erase(state.hand.begin() + card);
    if (!take.empty()) { --recipient.craftedItems.at(take); addItem(take); }
    auto& relation = state.relationships.at(factionIndex(target));
    auto& aided = state.aids.at(factionIndex(target));
    if (relation == 3) awardPoints(faction, 2);
    else if (relation >= 0 && ++aided >= relation + 1) {
        aided = 0;
        ++relation;
        awardPoints(faction, relation == 1 ? 1 : 2);
    }
    state.phaseStarted = true;
    syncItems();
}

void SingleGame::aid(FactionId faction, FactionId target, int clearing) {
    const auto& cards = hand(faction);
    for (std::size_t index = 0; index < cards.size(); ++index) {
        if (cards[index].getCardSuit() != map_.clearing(clearing).suit() && cards[index].getCardSuit() != Suit::Bird) continue;
        for (const auto& item : inventory_) if (!item.damaged && !item.exhausted) {
            aidChosen(faction, target, clearing, static_cast<int>(index), item.id, "");
            return;
        }
    }
    throw std::logic_error("Aid needs a matching card and a ready item");
}

void SingleGame::repair(FactionId faction) {
    requireFactionPhase(faction, Phase::Daylight);
    if (faction != FactionId::Vagabond) throw std::logic_error("Only the Vagabond repairs items");
    const auto damaged = std::find_if(inventory_.begin(), inventory_.end(), [](const auto& item) { return item.damaged; });
    if (damaged == inventory_.end()) throw std::logic_error("No damaged item to repair");
    const int id = damaged->id;
    exhaustItem("hammer");
    itemById(id).damaged = false;
    player(faction).phaseStarted = true;
    syncItems();
}

void SingleGame::strike(FactionId faction, FactionId target, int clearing, const std::string& piece) {
    requireFactionPhase(faction, Phase::Daylight);
    if (faction != FactionId::Vagabond || target == faction || target == FactionId::Count || vagabondLocation() != clearing) throw std::logic_error("Strike in your clearing");
    player(target);
    const auto choices = lossChoices(target, clearing);
    if (std::none_of(choices.begin(), choices.end(), [&](const auto& entry) { return entry.value == piece; })) throw std::logic_error("Choose an enemy warrior before cardboard");
    exhaustItem("crossbow");
    removeChosenPiece(target, clearing, piece, faction);
    player(faction).phaseStarted = true;
}

void SingleGame::applyRuleAction(FactionId faction, const RuleAction& action) {
    if (action.type == "character") {
        requireSetupStep(faction, "choose_character");
        if (action.option != "Thief" && action.option != "Tinker" && action.option != "Ranger") throw std::logic_error("Unknown base character");
        character_ = action.option;
        inventory_.clear();
        addItem("boot"); addItem("torch");
        if (character_ == "Thief") { addItem("tea"); addItem("sword"); }
        if (character_ == "Tinker") { addItem("bag"); addItem("hammer"); }
        if (character_ == "Ranger") { addItem("crossbow"); addItem("sword"); }
        setup_.step = "place_pawn";
    } else if (action.type == "pawn") {
        requireSetupStep(faction, "place_pawn");
        if (action.clearing < 12 || action.clearing >= 19) throw std::logic_error("Start in a forest");
        forest_ = action.clearing;
        finishSetup();
    } else if (action.type == "effect") useEffect(faction, action.card, action.target, action.clearing);
    else if (action.type == "dominance") activateDominance(faction, action.card, action.target);
    else if (action.type == "take_dominance") {
        requireFactionPhase(faction, Phase::Daylight);
        const auto card = dominanceCards_.at(action.item);
        const auto& payment = hand(faction).at(action.card);
        if (payment.getCardSuit() != card.getCardSuit() && payment.getCardSuit() != Suit::Bird) throw std::logic_error("Pay a matching card");
        dominanceCards_.erase(dominanceCards_.begin() + action.item);
        discardHand(player(faction), action.card);
        player(faction).hand.push_back(card);
    } else if (action.type == "bonus_battle" || action.type == "bonus_move") {
        requireFactionPhase(faction, action.type == "bonus_battle" ? Phase::Daylight : Phase::Evening);
        auto& state = player(faction);
        const std::string effect = action.type == "bonus_battle" ? "Command Warren" : "Cobbler";
        if (!hasEffect(faction, effect) || std::find(state.effectsUsed.begin(), state.effectsUsed.end(), effect) != state.effectsUsed.end()
            || eveningCardDrawn_) throw std::logic_error("This bonus action is unavailable");
        if (state.phaseStarted) throw std::logic_error("Use this bonus at the start of its phase");
        effectAction_ = true;
        try {
            if (action.type == "bonus_battle") beginBattle(faction, action.target, action.clearing);
            else move(faction, action.clearing, std::stoi(action.option), action.item);
        } catch (...) { effectAction_ = false; throw; }
        effectAction_ = false;
        state.effectsUsed.push_back(effect);
    } else if (action.type == "refresh") {
        requireFactionPhase(faction, Phase::Birdsong);
        auto& item = itemById(action.item);
        if (faction != FactionId::Vagabond || slipped_ || refreshes_ <= 0 || !item.exhausted) throw std::logic_error("Cannot refresh that item");
        item.exhausted = false;
        --refreshes_;
        player(faction).phaseStarted = true;
        syncItems();
    } else if (action.type == "slip") slip(faction, action.clearing);
    else if (action.type == "forest_move") move(faction, vagabondLocation(), action.clearing, 1);
    else if (action.type == "allied_move") {
        requireFactionPhase(faction, Phase::Daylight);
        const int from = vagabondLocation();
        const int to = action.clearing;
        if (faction != FactionId::Vagabond || action.target == faction || action.target == FactionId::Count
            || from < 0 || from >= 12 || to >= 12 || relationship(action.target) != 3 || action.item <= 0
            || map_.clearing(from).pieces(action.target, PieceType::Warrior) < action.item
            || !map_.canMove(action.target, from, to)) throw std::logic_error("Move Allied warriors along a legal path with your pawn");
        move(faction, from, to, 1);
        map_.clearing(from).remove(action.target, PieceType::Warrior, action.item);
        map_.clearing(to).add(action.target, PieceType::Warrior, action.item);
        if (map_.clearing(to).pieces(FactionId::WoodlandAlliance, PieceType::Token) > 0) triggerOutrage(faction, map_.clearing(to).suit());
    }
    else if (action.type == "aid") aidChosen(faction, action.target, action.clearing, action.card, action.item, action.option);
    else if (action.type == "strike") strike(faction, action.target, action.clearing, action.option);
    else if (action.type == "repair") {
        requireFactionPhase(faction, Phase::Daylight);
        if (!itemById(action.item).damaged) throw std::logic_error("Choose a damaged item");
        exhaustItem("hammer");
        itemById(action.item).damaged = false;
        player(faction).phaseStarted = true;
        syncItems();
    } else if (action.type == "discard_item") {
        requireFactionPhase(faction, Phase::Evening);
        if (!eveningCardDrawn_ || hand(faction).size() > 5 || satchelSize() <= itemCapacity() || onTrack(itemById(action.item))) throw std::logic_error("Item capacity is checked after discarding cards");
        inventory_.erase(std::remove_if(inventory_.begin(), inventory_.end(), [&](const auto& item) { return item.id == action.item; }), inventory_.end());
        syncItems();
    } else if (action.type == "quest") {
        requireFactionPhase(faction, Phase::Daylight);
        const auto& quest = questCards.at(action.card);
        const auto offered = std::find_if(quests_.begin(), quests_.end(), [&](const auto& entry) { return entry.value == std::to_string(action.card); });
        if (faction != FactionId::Vagabond || offered == quests_.end() || vagabondLocation() >= 12
            || map_.clearing(vagabondLocation()).suit() != quest.suit || readyItems(quest.first) == 0
            || readyItems(quest.second) < (std::string(quest.first) == quest.second ? 2 : 1)) throw std::logic_error("Quest needs its two ready items in a matching clearing");
        exhaustItem(quest.first); exhaustItem(quest.second);
        const int completed = ++completedQuests_.at(static_cast<std::size_t>(quest.suit));
        if (action.option == "cards") drawCards(player(faction), 2);
        else awardPoints(faction, completed);
        player(faction).phaseStarted = true;
        quests_.erase(offered);
        if (!questDeck_.empty()) {
            const int next = questDeck_.back(); questDeck_.pop_back();
            quests_.push_back({std::to_string(next), questCards[next].name});
        }
    } else if (action.type == "special") {
        requireFactionPhase(faction, Phase::Daylight);
        if (faction != FactionId::Vagabond || vagabondLocation() >= 12) throw std::logic_error("Use your character ability in a clearing");
        if (character_ == "Thief") {
            const auto& land = map_.clearing(vagabondLocation());
            auto& victim = player(action.target);
            if (action.target == faction || victim.hand.empty() || land.ruleStrength(action.target) + land.pieces(action.target, PieceType::Token) == 0) throw std::logic_error("Choose a player in your clearing");
            exhaustItem("torch");
            std::uniform_int_distribution<std::size_t> pick(0, victim.hand.size() - 1);
            const auto index = pick(random_);
            player(faction).hand.push_back(victim.hand[index]);
            victim.hand.erase(victim.hand.begin() + static_cast<std::ptrdiff_t>(index));
            player(faction).phaseStarted = true;
        } else if (character_ == "Tinker") {
            const auto card = discardPile_.at(action.card);
            if (card.getCardSuit() != Suit::Bird && card.getCardSuit() != map_.clearing(vagabondLocation()).suit()) throw std::logic_error("Day Labor needs a matching discard");
            exhaustItem("torch");
            player(faction).hand.push_back(card);
            discardPile_.erase(discardPile_.begin() + action.card);
            player(faction).phaseStarted = true;
        } else if (character_ == "Ranger") {
            exhaustItem("torch");
            std::vector<RuleChoice> choices;
            for (const auto& item : inventory_) if (item.damaged) choices.push_back({std::to_string(item.id), item.type + " #" + std::to_string(item.id)});
            if (choices.empty()) advancePhase();
            else decisions_.emplace_back(faction, "hideout_repair", "Hideout: choose up to three damaged items", choices, -1, 3);
        } else throw std::logic_error("Choose a character first");
    } else throw std::logic_error("Unknown rule action");
}

std::vector<RuleAction> SingleGame::ruleActions(FactionId faction) const {
    std::vector<RuleAction> actions;
    if (faction != activeFaction() || winner_ || !decisions_.empty() || battle_) return actions;
    const auto add = [&](RuleAction action) {
        try { auto preview = *this; preview.applyRuleAction(faction, action); actions.push_back(std::move(action)); }
        catch (const std::exception&) {}
    };
    if (phase_ == Phase::Setup) {
        if (setup_.step == "choose_character") for (const char* character : {"Thief", "Tinker", "Ranger"}) add({"character", character, -1, -1, -1, FactionId::Count, character});
        if (setup_.step == "place_pawn") for (int id = 12; id < 19; ++id) add({"pawn", "Start in forest " + std::to_string(id - 11), -1, -1, id});
        return actions;
    }
    const auto& state = player(faction);
    for (std::size_t index = 0; index < state.persistent.size(); ++index) {
        const auto& name = state.persistent[index].getCardName();
        if (name == "Royal Claim") add({"effect", name, static_cast<int>(index)});
        else if (name == "Tax Collector") for (int id = 0; id < 12; ++id) add({"effect", name + " - " + map_.clearing(id).name(), static_cast<int>(index), -1, id});
        else for (const auto& other : players_) if (other.faction.id() != faction) add({"effect", name + " - " + std::string(other.faction.name()), static_cast<int>(index), -1, -1, other.faction.id()});
    }
    for (std::size_t index = 0; index < state.hand.size(); ++index) if (state.hand[index].getEffectType() == ShareCard::CardEffectType::Dominance) {
        if (faction == FactionId::Vagabond) {
            for (const auto& other : players_) if (other.faction.id() != faction) add({"dominance", "Coalition - " + std::string(other.faction.name()), static_cast<int>(index), -1, -1, other.faction.id()});
        }
        else add({"dominance", "Activate " + state.hand[index].getCardName(), static_cast<int>(index)});
    }
    for (std::size_t index = 0; index < dominanceCards_.size(); ++index) for (std::size_t card = 0; card < state.hand.size(); ++card) {
        add({"take_dominance", "Take " + dominanceCards_[index].getCardName() + " / " + state.hand[card].getCardName(), static_cast<int>(card), static_cast<int>(index)});
    }
    if (hasEffect(faction, "Cobbler") || hasEffect(faction, "Command Warren")) for (int id = 0; id < 12; ++id) {
        for (const auto& other : players_) if (other.faction.id() != faction) add({"bonus_battle", "Command Warren - " + map_.clearing(id).name() + " / " + std::string(other.faction.name()), -1, -1, id, other.faction.id()});
        for (const int to : map_.neighbors(id)) for (int count = 1; count <= map_.clearing(id).pieces(faction, PieceType::Warrior); ++count) {
            add({"bonus_move", "Cobbler - " + std::to_string(count) + " / " + map_.clearing(id).name() + " > " + map_.clearing(to).name(), -1, count, id, FactionId::Count, std::to_string(to)});
        }
    }
    if (faction != FactionId::Vagabond) return actions;
    const int location = vagabondLocation();
    for (int to = 0; to < 19; ++to) {
        add({"slip", "Slip - " + (to < 12 ? map_.clearing(to).name() : "Forest " + std::to_string(to - 11)), -1, -1, to});
        if (location >= 12 && to < 12) add({"forest_move", "Move - " + map_.clearing(to).name(), -1, -1, to});
    }
    for (const auto& item : inventory_) {
        const auto label = item.type + " #" + std::to_string(item.id);
        add({"refresh", "Refresh " + label, -1, item.id});
        add({"repair", "Repair " + label, -1, item.id});
        add({"discard_item", "Discard " + label, -1, item.id});
        if (location < 0 || location >= 12 || item.damaged || item.exhausted) continue;
        for (const auto& target : players_) if (target.faction.id() != faction) {
            for (std::size_t card = 0; card < state.hand.size(); ++card) {
                const auto labelAid = "Aid " + std::string(target.faction.name()) + " / " + state.hand[card].getCardName() + " / " + label;
                add({"aid", labelAid, static_cast<int>(card), item.id, location, target.faction.id()});
                for (const auto& [type, count] : target.craftedItems) if (count > 0) add({"aid", labelAid + " / take " + type, static_cast<int>(card), item.id, location, target.faction.id(), type});
            }
        }
    }
    if (location >= 0 && location < 12) {
        for (const auto& target : players_) if (target.faction.id() != faction) {
            if (relationship(target.faction.id()) == 3) for (const int to : map_.neighbors(location)) {
                for (int count = 1; count <= map_.clearing(location).pieces(target.faction.id(), PieceType::Warrior); ++count) {
                    add({"allied_move", "Move with " + std::string(target.faction.name()) + " / " + std::to_string(count) + " / " + map_.clearing(to).name(), -1, count, to, target.faction.id()});
                }
            }
            for (const auto& piece : lossChoices(target.faction.id(), location)) add({"strike", "Strike " + std::string(target.faction.name()) + " / " + piece.label, -1, -1, location, target.faction.id(), piece.value});
            if (character_ == "Thief") add({"special", "Steal - " + std::string(target.faction.name()), -1, -1, location, target.faction.id()});
        }
        for (const auto& quest : quests_) for (const char* reward : {"points", "cards"}) add({"quest", quest.label + " / " + reward, std::stoi(quest.value), -1, location, FactionId::Count, reward});
        if (character_ == "Tinker") for (std::size_t index = 0; index < discardPile_.size(); ++index) add({"special", "Day Labor - " + discardPile_[index].getCardName(), static_cast<int>(index)});
        if (character_ == "Ranger") add({"special", "Hideout"});
    }
    return actions;
}
