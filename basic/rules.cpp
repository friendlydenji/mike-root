#include "game.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>

namespace {
bool matches(const ShareCard& card, Suit suit) {
    return card.getCardSuit() == suit || card.getCardSuit() == Suit::Bird;
}
const std::unordered_map<std::string, std::pair<std::string, int>> crafts{
    {"Woodland Runners", {"boot", 1}}, {"Travel Gear", {"boot", 1}}, {"A Visit to Friends", {"boot", 1}},
    {"Crossbow", {"crossbow", 1}}, {"Birdy Bindle", {"bag", 1}}, {"Gently Used Knapsack", {"bag", 1}},
    {"Mouse in a Sack", {"bag", 1}}, {"Smuggler's Trail", {"bag", 1}},
    {"Arms Trader", {"sword", 2}}, {"Foxfolk Steel", {"sword", 2}}, {"Sword", {"sword", 2}},
    {"Anvil", {"hammer", 2}}, {"Root Tea", {"tea", 2}},
    {"Protection Racket", {"coins", 3}}, {"Investments", {"coins", 3}}, {"Bake Sale", {"coins", 3}}
};
}

void SingleGame::recycleDeck() {
    if (!drawPile_.empty() || discardPile_.empty()) return;
    drawPile_.swap(discardPile_);
    std::shuffle(drawPile_.begin(), drawPile_.end(), random_);
}

void SingleGame::discard(ShareCard card) {
    if (card.getEffectType() == ShareCard::CardEffectType::Dominance) dominanceCards_.push_back(std::move(card));
    else discardPile_.push_back(std::move(card));
    recycleDeck();
}

void SingleGame::discardHand(PlayerState& state, std::size_t index) {
    const auto card = state.hand.at(index);
    state.hand.erase(state.hand.begin() + static_cast<std::ptrdiff_t>(index));
    discard(card);
}

bool SingleGame::canPlace(FactionId faction, int clearing) const {
    return faction == FactionId::MarquiseDeCat || map_.clearing(clearing).keep() == 0;
}

int SingleGame::allianceSupply() const {
    return std::max(0, 10 - pieceCount(FactionId::WoodlandAlliance, PieceType::Warrior)
                           - player(FactionId::WoodlandAlliance).officers);
}

void SingleGame::baseLost(Suit suit) {
    auto& alliance = player(FactionId::WoodlandAlliance);
    for (std::size_t index = alliance.supporters.size(); index-- > 0;) {
        if (matches(alliance.supporters[index], suit)) {
            discard(alliance.supporters[index]);
            alliance.supporters.erase(alliance.supporters.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }
    alliance.officers /= 2;
    if (pieceCount(FactionId::WoodlandAlliance, PieceType::Building) == 0 && alliance.supporters.size() > 5) {
        std::vector<RuleChoice> choices;
        for (std::size_t index = 0; index < alliance.supporters.size(); ++index) {
            choices.push_back({std::to_string(index), alliance.supporters[index].getCardName()});
        }
        decisions_.emplace_back(FactionId::WoodlandAlliance, "supporter_limit", "Discard supporters down to five", choices);
    }
}

void SingleGame::triggerOutrage(FactionId offender, Suit suit) {
    if (!hasFaction(FactionId::WoodlandAlliance) || offender == FactionId::WoodlandAlliance) return;
    const auto& cards = hand(offender);
    std::vector<RuleChoice> choices;
    for (std::size_t index = 0; index < cards.size(); ++index) if (matches(cards[index], suit)) {
        choices.push_back({std::to_string(index), cards[index].getCardName()});
    }
    if (!choices.empty()) {
        decisions_.emplace_back(offender, "outrage", "Outrage: give a matching card", choices, -1, 0, suit);
    } else {
        std::string title = std::string(factionName(offender)) + " reveals: ";
        for (const auto& card : cards) title += card.getCardName() + "; ";
        if (cards.empty()) title += "empty hand";
        decisions_.emplace_back(FactionId::WoodlandAlliance, "reveal", title, std::vector<RuleChoice>{{"pass", "Draw a supporter"}});
    }
}

void SingleGame::newRoost(FactionId faction, int clearing) {
    requireFactionPhase(faction, Phase::Birdsong);
    if (faction != FactionId::EyrieDynasties || eyrieState().cardsAddedThisBirdsong == 0
        || pieceCount(faction, PieceType::Building) != 0) throw std::logic_error("A New Roost is not available");
    int minimum = 100;
    for (int id = 0; id < 12; ++id) {
        const auto& land = map_.clearing(id);
        if (!canPlace(faction, id) || land.usedBuildingSlots() + (land.hasRuin() ? 1 : 0) >= land.buildingSlots()) continue;
        int warriors = 0;
        for (const auto& state : players_) if (state.faction.id() != FactionId::Vagabond) warriors += land.pieces(state.faction.id(), PieceType::Warrior);
        minimum = std::min(minimum, warriors);
    }
    auto& land = map_.clearing(clearing);
    int warriors = 0;
    for (const auto& state : players_) if (state.faction.id() != FactionId::Vagabond) warriors += land.pieces(state.faction.id(), PieceType::Warrior);
    if (!canPlace(faction, clearing) || warriors != minimum
        || land.usedBuildingSlots() + (land.hasRuin() ? 1 : 0) >= land.buildingSlots()) {
        throw std::logic_error("Choose an eligible clearing with the fewest warriors");
    }
    land.add(faction, PieceType::Building);
    const int supply = std::min(3, 20 - pieceCount(faction, PieceType::Warrior));
    if (supply > 0) land.add(faction, PieceType::Warrior, supply);
}

const std::vector<ShareCard>& SingleGame::persistentCards(FactionId faction) const { return player(faction).persistent; }
std::optional<Suit> SingleGame::dominance(FactionId faction) const { return player(faction).dominance; }

bool SingleGame::hasEffect(FactionId faction, const std::string& name) const {
    const auto& cards = player(faction).persistent;
    return std::any_of(cards.begin(), cards.end(), [&](const auto& card) { return card.getCardName() == name; });
}

void SingleGame::craft(FactionId faction, std::size_t index) {
    requireFactionPhase(faction, Phase::Daylight);
    auto& state = player(faction);
    const auto card = state.hand.at(index);
    const auto reward = crafts.find(card.getCardName());
    const bool persistent = card.getEffectType() == ShareCard::CardEffectType::PersistentCrafting;
    const bool favor = card.getCardName().find("Favor of the ") == 0;
    if (!card.getCraftingIngredients() || (!persistent && !favor && reward == crafts.end())) {
        throw std::logic_error("This card cannot be crafted");
    }
    if (faction == FactionId::MarquiseDeCat && state.cat.daylightActionsStarted) {
        throw std::logic_error("Craft before taking Daylight actions");
    }
    if (faction == FactionId::EyrieDynasties) for (const auto& column : state.eyrie.decree) {
        for (const auto& entry : column) if (entry.resolved) throw std::logic_error("Craft before the Decree");
    }
    if (persistent && hasEffect(faction, card.getCardName())) throw std::logic_error("That persistent effect is already crafted");
    if (reward != crafts.end() && itemSupply_.at(reward->second.first) == 0) throw std::logic_error("Item supply is empty");
    auto used = state.craftingUsed;
    if (faction == FactionId::Vagabond) {
        const int location = vagabondLocation();
        if (location >= 12 || readyItems("hammer") < static_cast<int>(card.getCraftingIngredients()->size())) {
            throw std::logic_error("Crafting needs ready hammers in a clearing");
        }
        for (const auto suit : *card.getCraftingIngredients()) {
            if (suit != Suit::Any && suit != map_.clearing(location).suit()) throw std::logic_error("Clearing must match every crafting symbol");
        }
    } else {
        // Spend specific suits first; wild symbols must not consume a required suited piece.
        auto ingredients = *card.getCraftingIngredients();
        std::stable_sort(ingredients.begin(), ingredients.end(), [](Suit a, Suit b) { return a != Suit::Any && b == Suit::Any; });
        for (const auto suit : ingredients) {
            int chosen = -1;
            for (int id = 0; id < 12; ++id) {
                const auto& land = map_.clearing(id);
                const int pieces = faction == FactionId::MarquiseDeCat ? land.catBuildings(CatBuilding::Workshop)
                    : land.pieces(faction, faction == FactionId::WoodlandAlliance ? PieceType::Token : PieceType::Building);
                if ((suit == Suit::Any || land.suit() == suit) && pieces > used[id]) { chosen = id; break; }
            }
            if (chosen < 0) throw std::logic_error("Insufficient unused matching crafting pieces");
            ++used[chosen];
        }
    }
    if (faction == FactionId::Vagabond) exhaustItem("hammer", static_cast<int>(card.getCraftingIngredients()->size()));
    else state.craftingUsed = used;
    state.phaseStarted = true;
    if (persistent) {
        state.persistent.push_back(card);
        state.hand.erase(state.hand.begin() + static_cast<std::ptrdiff_t>(index));
    } else {
        discardHand(state, index);
        if (reward != crafts.end()) {
            const auto& [item, points] = reward->second;
            --itemSupply_.at(item);
            if (faction == FactionId::Vagabond) addItem(item);
            else ++state.craftedItems[item];
            awardPoints(faction, faction == FactionId::EyrieDynasties && state.eyrie.leader != "Builder" ? 1 : points);
        } else {
            for (int id = 0; id < 12; ++id) {
                auto& land = map_.clearing(id);
                if (land.suit() != card.getCardSuit()) continue;
                for (const auto& enemy : players_) {
                    const auto target = enemy.faction.id();
                    if (target == faction) continue;
                    const int hits = target == FactionId::Vagabond ? (land.pieces(target, PieceType::Warrior) ? 3 : 0)
                        : land.pieces(target, PieceType::Warrior) + land.pieces(target, PieceType::Building) + land.pieces(target, PieceType::Token);
                    if (target == FactionId::Vagabond) queueLosses(target, id, hits, faction);
                    else {
                        const int warriors = land.pieces(target, PieceType::Warrior);
                        if (warriors) land.remove(target, PieceType::Warrior, warriors);
                        const auto pieces = lossChoices(target, id);
                        for (const auto& piece : pieces) {
                            while (true) {
                                const auto remaining = lossChoices(target, id);
                                if (std::none_of(remaining.begin(), remaining.end(), [&](const auto& option) { return option.value == piece.value; })) break;
                                removeChosenPiece(target, id, piece.value, faction);
                            }
                        }
                        if (target == FactionId::MarquiseDeCat) hospitals(id, warriors);
                    }
                }
            }
        }
    }
}

void SingleGame::activateDominance(FactionId faction, std::size_t card, FactionId target) {
    requireFactionPhase(faction, Phase::Daylight);
    auto& state = player(faction);
    const auto dominanceCard = state.hand.at(card);
    if (players_.size() < 3 || state.score < 10 || state.dominance
        || dominanceCard.getEffectType() != ShareCard::CardEffectType::Dominance) throw std::logic_error("Dominance requires ten points and an inactive victory condition");
    if (faction == FactionId::Vagabond) {
        if (players_.size() < 4 || target == faction || target == FactionId::Count) throw std::logic_error("Coalitions require four players");
        int lowest = 100;
        for (const auto& other : players_) if (other.faction.id() != faction) lowest = std::min(lowest, other.score);
        if (player(target).score != lowest) throw std::logic_error("Choose a lowest-scoring faction");
        coalition_ = target;
        if (state.relationships.at(factionIndex(target)) < 0) state.relationships.at(factionIndex(target)) = 0;
    }
    state.dominance = dominanceCard.getCardSuit();
    state.hand.erase(state.hand.begin() + static_cast<std::ptrdiff_t>(card));
}

void SingleGame::checkDominance() {
    auto& state = player(activeFaction());
    if (!state.dominance || activeFaction() == FactionId::Vagabond) return;
    bool won = false;
    if (*state.dominance == Suit::Bird) {
        won = (map_.rules(activeFaction(), 0) && map_.rules(activeFaction(), 9))
            || (map_.rules(activeFaction(), 2) && map_.rules(activeFaction(), 7));
    } else {
        int ruled = 0;
        for (int id = 0; id < 12; ++id) if (map_.clearing(id).suit() == *state.dominance && map_.rules(activeFaction(), id)) ++ruled;
        won = ruled >= 3;
    }
    if (won) winner_ = activeFaction();
}

void SingleGame::useEffect(FactionId faction, int index, FactionId target, int clearing) {
    requireFactionPhase(faction, phase_);
    auto& state = player(faction);
    const auto name = state.persistent.at(index).getCardName();
    if (std::find(state.effectsUsed.begin(), state.effectsUsed.end(), name) != state.effectsUsed.end()) throw std::logic_error("Effect already used this turn");
    if (name == "Better Burrow Bank" || name == "Stand and Deliver" || name == "Royal Claim") {
        if (phase_ != Phase::Birdsong || state.phaseStarted) {
            throw std::logic_error("Use this effect at the start of Birdsong");
        }
        if (name != "Royal Claim" && (target == faction || target == FactionId::Count)) throw std::logic_error("Choose another faction");
        if (name == "Better Burrow Bank") { drawCards(state, 1); drawCards(player(target), 1); }
        else if (name == "Stand and Deliver") {
            auto& victim = player(target);
            if (victim.hand.empty()) throw std::logic_error("Target has no cards");
            std::uniform_int_distribution<std::size_t> pick(0, victim.hand.size() - 1);
            const auto stolen = pick(random_);
            state.hand.push_back(victim.hand[stolen]);
            victim.hand.erase(victim.hand.begin() + static_cast<std::ptrdiff_t>(stolen));
            awardPoints(target, 1);
        } else {
            int ruled = 0;
            for (int id = 0; id < 12; ++id) if (map_.rules(faction, id)) ++ruled;
            discard(state.persistent.at(index));
            state.persistent.erase(state.persistent.begin() + index);
            awardPoints(faction, ruled);
        }
    } else if (name == "Tax Collector" || name == "Codebreakers") {
        if (phase_ != Phase::Daylight) throw std::logic_error("Use this effect in Daylight");
        if (name == "Tax Collector") {
            auto& land = map_.clearing(clearing);
            if (faction == FactionId::Vagabond || land.pieces(faction, PieceType::Warrior) == 0) throw std::logic_error("Choose one of your warriors");
            land.remove(faction, PieceType::Warrior);
            drawCards(state, 1);
            if (faction == FactionId::MarquiseDeCat) hospitals(clearing, 1);
        } else {
            if (target == faction || target == FactionId::Count) throw std::logic_error("Choose another player");
            std::string title = std::string(factionName(target)) + ": ";
            for (const auto& card : hand(target)) title += card.getCardName() + "; ";
            decisions_.emplace_back(faction, "inspect", title, std::vector<RuleChoice>{{"pass", "Close"}});
        }
    } else throw std::logic_error("This effect is used in a move or battle");
    state.effectsUsed.push_back(name);
    if (phase_ == Phase::Daylight) state.phaseStarted = true;
}

void SingleGame::executeRuleAction(FactionId faction, std::size_t index) {
    const auto actions = ruleActions(faction);
    if (index >= actions.size()) throw std::logic_error("That action is no longer available");
    applyRuleAction(faction, actions[index]);
}
