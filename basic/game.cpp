#include "game.hpp"

#include "card/share_deck.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <unordered_set>
#include <unordered_map>

namespace {
bool cardMatches(const ShareCard& card, Suit suit) {
    return card.getCardSuit() == suit || card.getCardSuit() == Suit::Bird;
}

constexpr std::array<int, 10> SympathyPoints{0, 1, 1, 1, 2, 2, 3, 4, 4, 4};
constexpr std::array<int, 10> SympathyCosts{1, 1, 1, 2, 2, 2, 3, 3, 3, 3};
constexpr std::array<int, 6> CatBuildCosts{0, 1, 2, 3, 3, 4};
constexpr std::array<std::array<int, 6>, 3> CatBuildPoints{{
    {0, 1, 2, 3, 4, 5}, {0, 2, 2, 3, 4, 5}, {0, 1, 2, 3, 3, 4}
}};

int oppositeCorner(int clearingId) {
    switch (clearingId) {
        case 0: return 9;
        case 9: return 0;
        case 2: return 7;
        case 7: return 2;
        default: throw std::logic_error("Choose a corner clearing");
    }
}
}

SingleGame::SingleGame(std::vector<FactionId> factions, unsigned seed, SetupMode setupMode)
    : map_(Map::autumn()), random_(seed), setupMode_(setupMode) {
    if (factions.size() < 2 || factions.size() > 4) {
        throw std::invalid_argument("Root needs between two and four factions");
    }
    std::unordered_set<int> unique;
    for (const auto faction : factions) {
        if (faction == FactionId::Count || !unique.insert(static_cast<int>(faction)).second) {
            throw std::invalid_argument("Every faction in a game must be unique");
        }
        players_.emplace_back(faction);
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
    if (phase_ == Phase::Setup) return setup_.faction.value();
    return players_.at(activePlayer_).faction.id();
}

int SingleGame::score(FactionId faction) const {
    return player(faction).score;
}

const std::vector<ShareCard>& SingleGame::hand(FactionId faction) const {
    return player(faction).hand;
}

const std::vector<ShareCard>& SingleGame::supporters(FactionId faction) const {
    return player(faction).supporters;
}

int SingleGame::officers(FactionId faction) const {
    return player(faction).officers;
}

int SingleGame::sympathyPlaced() const {
    int count = 0;
    for (int id = 0; id < static_cast<int>(map_.size()); ++id) {
        count += map_.clearing(id).pieces(FactionId::WoodlandAlliance, PieceType::Token);
    }
    return count;
}

int SingleGame::sympathyCost(int clearingId) const {
    const int placed = sympathyPlaced();
    int cost = SympathyCosts.at(static_cast<std::size_t>(std::min(placed, 9)));
    const auto& land = map_.clearing(clearingId);
    for (const auto faction : {FactionId::MarquiseDeCat, FactionId::EyrieDynasties,
                               FactionId::Vagabond}) {
        if (faction != FactionId::Vagabond
            && land.pieces(faction, PieceType::Warrior) >= 3) {
            ++cost;
            break;
        }
    }
    return cost;
}

const VagabondItems& SingleGame::vagabondItems() const {
    return player(FactionId::Vagabond).vagabond;
}

int SingleGame::relationship(FactionId faction) const {
    return player(FactionId::Vagabond).relationships.at(factionIndex(faction));
}

const EyrieState& SingleGame::eyrieState() const {
    return player(FactionId::EyrieDynasties).eyrie;
}

const CatState& SingleGame::catState() const {
    return player(FactionId::MarquiseDeCat).cat;
}

int SingleGame::catBuildingCount(CatBuilding type) const {
    int total = 0;
    for (int id = 0; id < static_cast<int>(map_.size()); ++id) total += map_.clearing(id).catBuildings(type);
    return total;
}

int SingleGame::catBuildCost(CatBuilding type) const {
    const int count = catBuildingCount(type);
    return count < 6 ? CatBuildCosts.at(static_cast<std::size_t>(count)) : -1;
}

int SingleGame::catWoodCount() const {
    int total = 0;
    for (int id = 0; id < static_cast<int>(map_.size()); ++id) total += map_.clearing(id).wood();
    return total;
}

void SingleGame::reset() {
    map_ = Map::autumn();
    drawPile_ = createShareDeck();
    if (players_.size() == 2) {
        drawPile_.erase(std::remove_if(drawPile_.begin(), drawPile_.end(), [](const auto& card) {
            return card.getEffectType() == ShareCard::CardEffectType::Dominance;
        }), drawPile_.end());
    }
    discardPile_.clear();
    dominanceCards_.clear();
    decisions_.clear();
    battle_.reset();
    completedBattle_.reset();
    coalition_.reset();
    effectAction_ = false;
    battleId_ = 0;
    initializeVagabond();
    itemSupply_ = {{"boot", 2}, {"bag", 2}, {"crossbow", 1}, {"hammer", 1}, {"sword", 2}, {"tea", 2}, {"coins", 2}};
    std::shuffle(drawPile_.begin(), drawPile_.end(), random_);
    for (auto& state : players_) {
        state.score = 0;
        state.hand.clear();
        state.supporters.clear();
        state.officers = 0;
        state.vagabond = {};
        state.eyrie = {};
        state.cat = {};
        state.craftedItems.clear();
        state.craftingUsed.fill(0);
        state.relationships.fill(0);
        state.aids.fill(0);
        state.persistent.clear();
        state.effectsUsed.clear();
        state.dominance.reset();
        state.spreadStarted = false;
        state.allianceDaylightStarted = false;
        state.phaseStarted = false;
        drawCards(state, 3);
    }
    activePlayer_ = 0;
    phase_ = Phase::Birdsong;
    actionsRemaining_ = 0;
    eveningCardDrawn_ = false;
    winner_.reset();
    setup_ = {};
    setupFactions();
    syncItems();
    if (phase_ != Phase::Setup && activeFaction() == FactionId::Vagabond) refreshVagabond();
}

void SingleGame::setupFactions() {
    for (auto& state : players_) {
        switch (state.faction.id()) {
            case FactionId::MarquiseDeCat:
                if (setupMode_ == SetupMode::Interactive) break;
                for (int id = 0; id < static_cast<int>(map_.size()); ++id) {
                    if (id != 9) map_.clearing(id).add(FactionId::MarquiseDeCat, PieceType::Warrior);
                }
                map_.clearing(0).addCatBuilding(CatBuilding::Sawmill);
                map_.clearing(1).addCatBuilding(CatBuilding::Workshop);
                map_.clearing(3).addCatBuilding(CatBuilding::Recruiter);
                map_.clearing(0).add(FactionId::MarquiseDeCat, PieceType::Token);
                break;
            case FactionId::EyrieDynasties:
                if (setupMode_ == SetupMode::Interactive) break;
                map_.clearing(9).add(FactionId::EyrieDynasties, PieceType::Warrior, 6);
                map_.clearing(9).add(FactionId::EyrieDynasties, PieceType::Building);
                break;
            case FactionId::WoodlandAlliance:
                for (int count = 0; count < 3 && !drawPile_.empty(); ++count) {
                    gainSupporter(state, std::move(drawPile_.back()));
                    drawPile_.pop_back();
                }
                break;
            case FactionId::Vagabond:
                if (setupMode_ == SetupMode::Preset) {
                    map_.clearing(4).add(FactionId::Vagabond, PieceType::Warrior);
                    for (const auto* item : {"boot", "boot", "sword", "torch"}) addItem(item);
                }
                break;
            case FactionId::Count:
                break;
        }
    }
    if (setupMode_ == SetupMode::Interactive) {
        phase_ = Phase::Setup;
        if (hasFaction(FactionId::MarquiseDeCat)) {
            setup_.step = "place_keep";
            setup_.faction = FactionId::MarquiseDeCat;
        } else beginEyrieSetup();
    }
}

bool SingleGame::hasFaction(FactionId faction) const {
    return std::any_of(players_.begin(), players_.end(), [=](const auto& state) { return state.faction.id() == faction; });
}

void SingleGame::requireSetupStep(FactionId faction, const std::string& step) const {
    requireFactionPhase(faction, Phase::Setup);
    if (setup_.step != step) throw std::logic_error("This action is not the current setup step");
}

void SingleGame::finishSetup() {
    setup_.step = "complete";
    setup_.faction.reset();
    phase_ = Phase::Birdsong;
    if (activeFaction() == FactionId::Vagabond) refreshVagabond();
}

void SingleGame::beginEyrieSetup() {
    if (hasFaction(FactionId::EyrieDynasties)) {
        setup_.step = "place_roost";
        setup_.faction = FactionId::EyrieDynasties;
    } else finishEyrieSetup();
}

void SingleGame::placeSetupKeep(FactionId faction, int clearingId) {
    requireSetupStep(faction, "place_keep");
    const int opposite = oppositeCorner(clearingId);
    map_.clearing(clearingId).add(faction, PieceType::Token);
    for (int id = 0; id < static_cast<int>(map_.size()); ++id) {
        if (id != opposite) map_.clearing(id).add(faction, PieceType::Warrior);
    }
    setup_.keep = clearingId;
    setup_.step = "place_buildings";
}

void SingleGame::placeSetupCatBuilding(FactionId faction, CatBuilding type, int clearingId) {
    requireSetupStep(faction, "place_buildings");
    const auto index = static_cast<std::size_t>(type);
    if (setup_.buildingsPlaced.at(index)) throw std::logic_error("That starting building is already placed");
    if (clearingId != setup_.keep.value() && !map_.areAdjacent(setup_.keep.value(), clearingId)) {
        throw std::logic_error("Place starting buildings at the Keep or an adjacent clearing");
    }
    auto& land = map_.clearing(clearingId);
    if (land.usedBuildingSlots() + (land.hasRuin() ? 1 : 0) >= land.buildingSlots()) {
        throw std::logic_error("Choose an open building slot");
    }
    land.addCatBuilding(type);
    setup_.buildingsPlaced[index] = true;
    if (std::all_of(setup_.buildingsPlaced.begin(), setup_.buildingsPlaced.end(), [](bool placed) { return placed; })) {
        beginEyrieSetup();
    }
}

void SingleGame::placeSetupRoost(FactionId faction, int clearingId) {
    requireSetupStep(faction, "place_roost");
    oppositeCorner(clearingId);
    if (setup_.keep && clearingId != oppositeCorner(*setup_.keep)) {
        throw std::logic_error("Place the starting roost diagonally opposite the Keep");
    }
    auto& land = map_.clearing(clearingId);
    land.add(faction, PieceType::Building);
    land.add(faction, PieceType::Warrior, 6);
    setup_.eyrieCorner = clearingId;
    setup_.step = "choose_leader";
}

void SingleGame::advancePhase() {
    if (!decisions_.empty() || battle_) throw std::logic_error("Resolve the pending response first");
    if (phase_ == Phase::Setup) throw std::logic_error("Finish faction setup before starting turns");
    if (winner_) throw std::logic_error("The game is already over");
    if (phase_ == Phase::Birdsong) {
        if (activeFaction() == FactionId::Vagabond && refreshes_ > 0
            && std::any_of(inventory_.begin(), inventory_.end(), [](const auto& item) { return item.exhausted; })) {
            throw std::logic_error("Finish refreshing items before ending Birdsong");
        }
        if (activeFaction() == FactionId::MarquiseDeCat && !catState().woodProduced) {
            throw std::logic_error("Produce wood before ending Birdsong");
        }
        if (activeFaction() == FactionId::EyrieDynasties) {
            const auto& eyrie = player(FactionId::EyrieDynasties).eyrie;
            if (eyrie.leader.empty()) throw std::logic_error("Choose an Eyrie leader first");
            if (eyrie.cardsAddedThisBirdsong == 0) {
                throw std::logic_error("Add at least one card to the Decree");
            }
            if (pieceCount(FactionId::EyrieDynasties, PieceType::Building) == 0) {
                for (int id = 0; id < 12; ++id) {
                    const auto& land = map_.clearing(id);
                    if (canPlace(FactionId::EyrieDynasties, id)
                        && land.usedBuildingSlots() + (land.hasRuin() ? 1 : 0) < land.buildingSlots()) {
                        throw std::logic_error("Place A New Roost before ending Birdsong");
                    }
                }
            }
        }
        phase_ = Phase::Daylight;
        player(activeFaction()).phaseStarted = false;
        if (activeFaction() == FactionId::EyrieDynasties) {
            for (auto& column : player(FactionId::EyrieDynasties).eyrie.decree) {
                for (auto& card : column) card.resolved = false;
            }
            actionsRemaining_ = decreeSize();
        } else {
            actionsRemaining_ = activeFaction() == FactionId::MarquiseDeCat ? 3 : 0;
        }
    } else if (phase_ == Phase::Daylight) {
        if (activeFaction() == FactionId::EyrieDynasties) {
            if (actionsRemaining_ > 0) {
                for (int id = 0; id < static_cast<int>(map_.size()); ++id) {
                    if (!clearingOptions(FactionId::EyrieDynasties, id).actions.empty()) {
                        throw std::logic_error("Resolve the remaining Decree actions first");
                    }
                }
                auto& state = player(FactionId::EyrieDynasties);
                int birds = 0;
                for (auto& column : state.eyrie.decree) {
                    for (const auto& card : column) {
                        if (card.suit == Suit::Bird) ++birds;
                        if (card.original) discard(*card.original);
                    }
                    column.clear();
                }
                state.score = std::max(0, state.score - birds);
                state.eyrie.deposedLeaders.push_back(state.eyrie.leader);
                if (state.eyrie.deposedLeaders.size() == 4) state.eyrie.deposedLeaders.clear();
                state.eyrie.leader.clear();
                state.eyrie.needsLeaderAfterTurmoil = true;
                actionsRemaining_ = 0;
            }
        }
        phase_ = Phase::Evening;
        player(activeFaction()).phaseStarted = false;
        if (activeFaction() == FactionId::MarquiseDeCat) player(activeFaction()).cat.marchMovesRemaining = 0;
        actionsRemaining_ = activeFaction() == FactionId::WoodlandAlliance
            ? player(FactionId::WoodlandAlliance).officers : 0;
        eveningCardDrawn_ = false;
        if (activeFaction() == FactionId::EyrieDynasties) enterEyrieEvening();
    } else {
        if (activeFaction() == FactionId::EyrieDynasties && eyrieState().needsLeaderAfterTurmoil) {
            throw std::logic_error("Choose a new Eyrie leader after Turmoil");
        }
        if (!eveningCardDrawn_ && (!drawPile_.empty() || !discardPile_.empty())) {
            throw std::logic_error("Draw a card before ending Evening");
        }
        if (player(activeFaction()).hand.size() > 5) throw std::logic_error("Discard down to five cards before ending the turn");
        if (activeFaction() == FactionId::Vagabond && satchelSize() > itemCapacity()) {
            throw std::logic_error("Discard excess satchel items before ending the turn");
        }
        activePlayer_ = (activePlayer_ + 1) % players_.size();
        phase_ = Phase::Birdsong;
        actionsRemaining_ = 0;
        eveningCardDrawn_ = false;
        player(activeFaction()).craftingUsed.fill(0);
        player(activeFaction()).effectsUsed.clear();
        player(activeFaction()).spreadStarted = false;
        player(activeFaction()).allianceDaylightStarted = false;
        player(activeFaction()).phaseStarted = false;
        player(activeFaction()).aids.fill(0);
        checkDominance();
        if (activeFaction() == FactionId::Vagabond) refreshVagabond();
        if (activeFaction() == FactionId::MarquiseDeCat) player(activeFaction()).cat = {};
        if (activeFaction() == FactionId::EyrieDynasties) {
            auto& state = player(FactionId::EyrieDynasties);
            auto& eyrie = state.eyrie;
            eyrie.cardsAddedThisBirdsong = 0;
            eyrie.birdAddedThisBirdsong = false;
            if (state.hand.empty()) drawCards(state, 1);
        }
    }
}

bool SingleGame::canDrawCard(FactionId faction) const noexcept {
    return !winner_ && phase_ == Phase::Evening && activeFaction() == faction
        && !eveningCardDrawn_ && (!drawPile_.empty() || !discardPile_.empty())
        && decisions_.empty() && !battle_
        && (faction != FactionId::EyrieDynasties || !eyrieState().needsLeaderAfterTurmoil);
}

void SingleGame::drawCard(FactionId faction) {
    if (!canDrawCard(faction)) {
        throw std::logic_error("A card can only be drawn once during your Evening");
    }
    int amount = 1;
    if (faction == FactionId::EyrieDynasties) {
        const int roosts = pieceCount(faction, PieceType::Building);
        amount += (roosts >= 3 ? 1 : 0) + (roosts >= 6 ? 1 : 0);
    } else if (faction == FactionId::MarquiseDeCat) {
        const int recruiters = catBuildingCount(CatBuilding::Recruiter);
        amount += (recruiters >= 3 ? 1 : 0) + (recruiters >= 5 ? 1 : 0);
    } else if (faction == FactionId::WoodlandAlliance) {
        amount += pieceCount(faction, PieceType::Building);
        actionsRemaining_ = 0;
    } else if (faction == FactionId::Vagabond) {
        restVagabond();
        amount += std::min(3, readyItems("coins"));
    }
    drawCards(player(faction), amount);
    eveningCardDrawn_ = true;
    player(faction).phaseStarted = true;
}

void SingleGame::drawCards(PlayerState& state, int amount) {
    while (amount-- > 0) {
        recycleDeck();
        if (drawPile_.empty()) break;
        state.hand.push_back(std::move(drawPile_.back()));
        drawPile_.pop_back();
        recycleDeck();
    }
}

void SingleGame::requireFactionPhase(FactionId faction, Phase phase) const {
    if (winner_) throw std::logic_error("The game is already over");
    if (!decisions_.empty() || battle_) throw std::logic_error("Resolve the pending response first");
    if (activeFaction() != faction) throw std::logic_error("It is not this faction's turn");
    if (phase_ != phase) throw std::logic_error("This faction action is not available in this phase");
}

void SingleGame::requireAction(FactionId faction) const {
    if (effectAction_) { requireFactionPhase(faction, phase_); return; }
    const Phase required = faction == FactionId::WoodlandAlliance ? Phase::Evening : Phase::Daylight;
    requireFactionPhase(faction, required);
    if (faction == FactionId::WoodlandAlliance && eveningCardDrawn_) {
        throw std::logic_error("Military operations end before drawing cards");
    }
    if (faction != FactionId::Vagabond && actionsRemaining_ <= 0) {
        throw std::logic_error("No actions remain this turn");
    }
}

void SingleGame::consumeAction() {
    player(activeFaction()).phaseStarted = true;
    --actionsRemaining_;
    if (activeFaction() == FactionId::MarquiseDeCat) {
        auto& cat = player(activeFaction()).cat;
        cat.daylightActionsStarted = true;
        cat.marchMovesRemaining = 0;
    }
}

void SingleGame::move(FactionId faction, int from, int to, int warriors) {
    const bool continuingMarch = faction == FactionId::MarquiseDeCat && catState().marchMovesRemaining > 0;
    if (continuingMarch) requireFactionPhase(faction, Phase::Daylight);
    else requireAction(faction);
    if (warriors <= 0) throw std::invalid_argument("Move at least one warrior");
    if (faction == FactionId::Vagabond) {
        if (warriors != 1 || vagabondLocation() != from || to >= 12
            || !vagabondAdjacent(from, to)) {
            throw std::invalid_argument("The Vagabond pawn must move along one adjacent path");
        }
        int bootCost = 1;
        for (const auto enemy : {FactionId::MarquiseDeCat, FactionId::EyrieDynasties,
                                 FactionId::WoodlandAlliance}) {
            if (player(faction).relationships.at(factionIndex(enemy)) < 0
                && map_.clearing(to).pieces(enemy, PieceType::Warrior) > 0) {
                bootCost = 2;
                break;
            }
        }
        if (readyItems("boot") < bootCost) {
            throw std::logic_error("Moving into a hostile clearing needs two ready boots");
        }
        exhaustItem("boot", bootCost);
        if (from < 12) map_.clearing(from).remove(faction, PieceType::Warrior);
        forest_ = -1;
        map_.clearing(to).add(faction, PieceType::Warrior);
        player(faction).phaseStarted = true;
        return;
    }
    auto& source = map_.clearing(from);
    if (!map_.canMove(faction, from, to)) {
        throw std::invalid_argument("Move requires an adjacent road and rule at origin or destination");
    }
    if (source.pieces(faction, PieceType::Warrior) < warriors) {
        throw std::invalid_argument("Not enough warriors in the origin clearing");
    }
    const auto decreeCard = faction == FactionId::EyrieDynasties && !effectAction_
        ? requireDecree("Move", source.suit()) : 0;
    source.remove(faction, PieceType::Warrior, warriors);
    map_.clearing(to).add(faction, PieceType::Warrior, warriors);
    if (faction != FactionId::WoodlandAlliance && faction != FactionId::Vagabond
        && map_.clearing(to).pieces(FactionId::WoodlandAlliance, PieceType::Token) > 0) {
        triggerOutrage(faction, map_.clearing(to).suit());
    }
    if (!continuingMarch && !effectAction_) consumeAction();
    if (faction == FactionId::MarquiseDeCat && !effectAction_) {
        player(faction).cat.marchMovesRemaining = continuingMarch ? 0 : 1;
    }
    if (faction == FactionId::EyrieDynasties && !effectAction_) resolveDecree("Move", decreeCard);
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
    if (state.dominance || (faction == FactionId::Vagabond && coalition_)) return;
    state.score += points;
    if (state.score >= 30 && !winner_) winner_ = faction;
}

bool SingleGame::spendSupporters(PlayerState& state, Suit suit, int amount) {
    std::vector<std::size_t> selected;
    for (std::size_t index = 0; index < state.supporters.size() && static_cast<int>(selected.size()) < amount; ++index) {
        if (cardMatches(state.supporters[index], suit)) selected.push_back(index);
    }
    if (static_cast<int>(selected.size()) < amount) return false;
    for (auto found = selected.rbegin(); found != selected.rend(); ++found) {
        discard(std::move(state.supporters[*found]));
        state.supporters.erase(state.supporters.begin() + static_cast<std::ptrdiff_t>(*found));
    }
    return true;
}

void SingleGame::gainSupporter(PlayerState& state, ShareCard card) {
    bool hasBase = false;
    for (int id = 0; id < static_cast<int>(map_.size()); ++id) {
        hasBase = hasBase || map_.clearing(id).pieces(FactionId::WoodlandAlliance, PieceType::Building) > 0;
    }
    if (hasBase || state.supporters.size() < 5) state.supporters.push_back(std::move(card));
    else discard(std::move(card));
}


void SingleGame::spreadSympathy(FactionId faction, int clearingId) {
    requireFactionPhase(faction, Phase::Birdsong);
    if (faction != FactionId::WoodlandAlliance) throw std::logic_error("Only the Alliance spreads sympathy");
    if (!canPlace(faction, clearingId)) throw std::logic_error("Cannot place sympathy at the Keep");
    auto& land = map_.clearing(clearingId);
    if (land.pieces(faction, PieceType::Token) > 0) throw std::logic_error("This clearing is already sympathetic");
    const int placed = sympathyPlaced();
    if (placed >= 10) throw std::logic_error("All sympathy tokens are already on the map");
    if (placed > 0) {
        bool adjacent = false;
        for (const int neighbor : map_.neighbors(clearingId)) {
            adjacent = adjacent || map_.clearing(neighbor).pieces(faction, PieceType::Token) > 0;
        }
        if (!adjacent) throw std::logic_error("Sympathy must spread from an adjacent sympathetic clearing");
    }
    auto& alliance = player(faction);
    if (!spendSupporters(alliance, land.suit(), sympathyCost(clearingId))) {
        throw std::logic_error("Not enough matching supporters");
    }
    land.add(faction, PieceType::Token);
    alliance.spreadStarted = true;
    alliance.phaseStarted = true;
    awardPoints(faction, SympathyPoints.at(static_cast<std::size_t>(placed)));
}

void SingleGame::revolt(FactionId faction, int clearingId) {
    requireFactionPhase(faction, Phase::Birdsong);
    if (faction != FactionId::WoodlandAlliance) throw std::logic_error("Only the Alliance revolts");
    if (player(faction).spreadStarted) throw std::logic_error("Revolt before spreading sympathy");
    if (!canPlace(faction, clearingId)) throw std::logic_error("Cannot place a base at the Keep");
    auto& land = map_.clearing(clearingId);
    if (land.pieces(faction, PieceType::Token) == 0) throw std::logic_error("A revolt needs sympathy");
    for (int id = 0; id < static_cast<int>(map_.size()); ++id) {
        const auto& other = map_.clearing(id);
        if (other.suit() == land.suit() && other.pieces(faction, PieceType::Building) > 0) {
            throw std::logic_error("The matching Alliance base is already on the map");
        }
    }
    auto& alliance = player(faction);
    if (!spendSupporters(alliance, land.suit(), 2)) {
        throw std::logic_error("A revolt needs two matching supporters");
    }
    for (const auto enemy : {FactionId::MarquiseDeCat, FactionId::EyrieDynasties, FactionId::Vagabond}) {
        if (enemy == FactionId::Vagabond) {
            if (land.pieces(enemy, PieceType::Warrior) > 0) {
                queueLosses(enemy, clearingId, 3, faction);
            }
            continue;
        }
        const int warriors = land.pieces(enemy, PieceType::Warrior);
        const int buildings = land.pieces(enemy, PieceType::Building);
        const int tokens = land.pieces(enemy, PieceType::Token);
        if (warriors) land.remove(enemy, PieceType::Warrior, warriors);
        if (buildings) land.remove(enemy, PieceType::Building, buildings);
        if (tokens) land.remove(enemy, PieceType::Token, tokens);
        if (enemy == FactionId::MarquiseDeCat) hospitals(clearingId, warriors);
        awardPoints(faction, buildings + tokens);
    }
    land.add(faction, PieceType::Building);
    int matchingSympathy = 0;
    for (int id = 0; id < static_cast<int>(map_.size()); ++id) {
        const auto& clearing = map_.clearing(id);
        if (clearing.suit() == land.suit()) {
            matchingSympathy += clearing.pieces(faction, PieceType::Token);
        }
    }
    const int recruited = std::min(matchingSympathy, allianceSupply());
    if (recruited > 0) land.add(faction, PieceType::Warrior, recruited);
    if (allianceSupply() > 0) ++alliance.officers;
    alliance.phaseStarted = true;
}

void SingleGame::mobilize(FactionId faction, std::size_t cardIndex) {
    requireFactionPhase(faction, Phase::Daylight);
    if (faction != FactionId::WoodlandAlliance) throw std::logic_error("Only the Alliance mobilizes");
    auto& alliance = player(faction);
    if (cardIndex >= alliance.hand.size()) throw std::out_of_range("Unknown hand card");
    gainSupporter(alliance, std::move(alliance.hand[cardIndex]));
    alliance.allianceDaylightStarted = true;
    alliance.phaseStarted = true;
    alliance.hand.erase(alliance.hand.begin() + static_cast<std::ptrdiff_t>(cardIndex));
}

void SingleGame::train(FactionId faction, std::size_t cardIndex) {
    requireFactionPhase(faction, Phase::Daylight);
    if (faction != FactionId::WoodlandAlliance) throw std::logic_error("Only the Alliance trains officers");
    auto& alliance = player(faction);
    if (cardIndex >= alliance.hand.size()) throw std::out_of_range("Unknown hand card");
    bool matchingBase = false;
    for (int id = 0; id < static_cast<int>(map_.size()); ++id) {
        const auto& land = map_.clearing(id);
        matchingBase = matchingBase || (land.pieces(faction, PieceType::Building) > 0
            && cardMatches(alliance.hand[cardIndex], land.suit()));
    }
    if (!matchingBase) throw std::logic_error("Train with a card matching an Alliance base");
    if (allianceSupply() == 0) throw std::logic_error("No Alliance warrior remains in supply");
    discardHand(alliance, cardIndex);
    alliance.allianceDaylightStarted = true;
    alliance.phaseStarted = true;
    ++alliance.officers;
}

void SingleGame::recruit(FactionId faction, int clearingId) {
    requireAction(faction);
    if (faction == FactionId::MarquiseDeCat) {
        auto& cat = player(faction).cat;
        if (cat.recruited) throw std::logic_error("The Marquise recruits only once per turn");
        if (catBuildingCount(CatBuilding::Recruiter) == 0) throw std::logic_error("No recruiters on the map");
        int supply = 25 - pieceCount(faction, PieceType::Warrior);
        if (supply <= 0) throw std::logic_error("No Marquise warriors in supply");
        for (int id = 0; id < static_cast<int>(map_.size()) && supply > 0; ++id) {
            const int amount = std::min(supply, map_.clearing(id).catBuildings(CatBuilding::Recruiter));
            if (amount > 0) map_.clearing(id).add(faction, PieceType::Warrior, amount);
            supply -= amount;
        }
        cat.recruited = true;
        consumeAction();
        return;
    }
    if (faction == FactionId::EyrieDynasties) {
        auto& land = map_.clearing(clearingId);
        if (!canPlace(faction, clearingId)) throw std::logic_error("Cannot recruit at the Keep");
        if (land.pieces(faction, PieceType::Building) == 0) {
            throw std::logic_error("Recruit in a clearing with an Eyrie roost");
        }
        const auto cardIndex = requireDecree("Recruit", land.suit());
        const int amount = eyrieState().leader == "Charismatic" ? 2 : 1;
        if (pieceCount(faction, PieceType::Warrior) + amount > 20) {
            throw std::logic_error("Not enough Eyrie warriors in supply");
        }
        land.add(faction, PieceType::Warrior, amount);
        resolveDecree("Recruit", cardIndex);
        consumeAction();
        return;
    }
    if (faction != FactionId::WoodlandAlliance
        || map_.clearing(clearingId).pieces(faction, PieceType::Building) == 0) {
        throw std::logic_error("Recruit in a clearing with an Alliance base");
    }
    if (!canPlace(faction, clearingId) || allianceSupply() == 0) throw std::logic_error("No legal Alliance warrior placement");
    map_.clearing(clearingId).add(faction, PieceType::Warrior);
    consumeAction();
}

void SingleGame::produceWood(FactionId faction) {
    requireFactionPhase(faction, Phase::Birdsong);
    if (faction != FactionId::MarquiseDeCat) throw std::logic_error("Only the Marquise produces wood");
    auto& cat = player(faction).cat;
    if (cat.woodProduced) throw std::logic_error("Wood was already produced this Birdsong");
    int supply = 8 - catWoodCount();
    for (int id = 0; id < static_cast<int>(map_.size()) && supply > 0; ++id) {
        const int amount = std::min(supply, map_.clearing(id).catBuildings(CatBuilding::Sawmill));
        if (amount > 0) map_.clearing(id).addWood(amount);
        supply -= amount;
    }
    cat.woodProduced = true;
    player(faction).phaseStarted = true;
}

std::vector<int> SingleGame::connectedCatClearings(int clearingId) const {
    std::vector<int> connected;
    if (map_.ruler(clearingId) != FactionId::MarquiseDeCat) return connected;
    connected.push_back(clearingId);
    for (std::size_t index = 0; index < connected.size(); ++index) {
        for (const int neighbor : map_.neighbors(connected[index])) {
            if (map_.ruler(neighbor) == FactionId::MarquiseDeCat
                && std::find(connected.begin(), connected.end(), neighbor) == connected.end()) {
                connected.push_back(neighbor);
            }
        }
    }
    return connected;
}

void SingleGame::buildCatBuilding(FactionId faction, CatBuilding type, int clearingId) {
    requireAction(faction);
    if (faction != FactionId::MarquiseDeCat) throw std::logic_error("Only the Marquise builds these buildings");
    auto& land = map_.clearing(clearingId);
    const int count = catBuildingCount(type);
    if (count >= 6 || land.usedBuildingSlots() + (land.hasRuin() ? 1 : 0) >= land.buildingSlots()) {
        throw std::logic_error("The building needs an open slot and a piece in supply");
    }
    const auto connected = connectedCatClearings(clearingId);
    if (connected.empty()) throw std::logic_error("Build in a clearing ruled by the Marquise");
    int availableWood = 0;
    for (const int id : connected) availableWood += map_.clearing(id).wood();
    int cost = catBuildCost(type);
    if (availableWood < cost) throw std::logic_error("Not enough wood connected through ruled clearings");
    land.addCatBuilding(type);
    for (const int id : connected) {
        const int paid = std::min(cost, map_.clearing(id).wood());
        if (paid > 0) map_.clearing(id).removeWood(paid);
        cost -= paid;
        if (cost == 0) break;
    }
    awardPoints(faction, CatBuildPoints.at(static_cast<std::size_t>(type)).at(static_cast<std::size_t>(count)));
    consumeAction();
}

void SingleGame::overwork(FactionId faction, int clearingId, std::size_t cardIndex) {
    requireAction(faction);
    if (faction != FactionId::MarquiseDeCat) throw std::logic_error("Only the Marquise overworks sawmills");
    auto& state = player(faction);
    if (cardIndex >= state.hand.size()) throw std::out_of_range("Unknown hand card");
    auto& land = map_.clearing(clearingId);
    if (land.catBuildings(CatBuilding::Sawmill) == 0 || !cardMatches(state.hand[cardIndex], land.suit())) {
        throw std::logic_error("Overwork needs a sawmill and a matching hand card");
    }
    if (catWoodCount() >= 8) throw std::logic_error("No wood remains in supply");
    land.addWood();
    discardHand(state, cardIndex);
    consumeAction();
}

void SingleGame::spendBirdCard(FactionId faction, std::size_t cardIndex) {
    requireFactionPhase(faction, Phase::Daylight);
    if (faction != FactionId::MarquiseDeCat) throw std::logic_error("Only the Marquise buys extra actions with bird cards");
    auto& hand = player(faction).hand;
    if (cardIndex >= hand.size()) throw std::out_of_range("Unknown hand card");
    if (hand[cardIndex].getCardSuit() != Suit::Bird) throw std::logic_error("Spend a bird card for an extra action");
    discardHand(player(faction), cardIndex);
    ++actionsRemaining_;
}

void SingleGame::finishMarch(FactionId faction) {
    requireFactionPhase(faction, Phase::Daylight);
    if (faction != FactionId::MarquiseDeCat || catState().marchMovesRemaining == 0) {
        throw std::logic_error("No March is in progress");
    }
    player(faction).cat.marchMovesRemaining = 0;
}

void SingleGame::discardCard(FactionId faction, std::size_t cardIndex) {
    requireFactionPhase(faction, Phase::Evening);
    if (canDrawCard(faction)) throw std::logic_error("Draw before discarding down to five cards");
    auto& hand = player(faction).hand;
    if (hand.size() <= 5) throw std::logic_error("Discard only when above the five-card hand limit");
    if (cardIndex >= hand.size()) throw std::out_of_range("Unknown hand card");
    discardHand(player(faction), cardIndex);
}

const std::map<std::string, int>& SingleGame::craftedItems(FactionId faction) const {
    return player(faction).craftedItems;
}


std::vector<std::string> SingleGame::cardActions(FactionId faction, std::size_t cardIndex) const {
    std::vector<std::string> actions;
    if (cardIndex >= hand(faction).size() || activeFaction() != faction || winner_) return actions;
    const auto legal = [&](auto action) {
        try { auto preview = *this; action(preview); return true; }
        catch (const std::exception&) { return false; }
    };
    if (legal([&](auto& game) { game.craft(faction, cardIndex); })) actions.push_back("craft");
    if (legal([&](auto& game) { game.spendBirdCard(faction, cardIndex); })) actions.push_back("spend_bird");
    if (legal([&](auto& game) { game.mobilize(faction, cardIndex); })) actions.push_back("mobilize");
    if (legal([&](auto& game) { game.train(faction, cardIndex); })) actions.push_back("train");
    if (legal([&](auto& game) { game.addToDecree(faction, cardIndex, "Recruit"); })) actions.push_back("add_to_decree");
    if (legal([&](auto& game) { game.discardCard(faction, cardIndex); })) actions.push_back("discard_card");
    return actions;
}

void SingleGame::buildRoost(FactionId faction, int clearingId) {
    requireAction(faction);
    if (faction != FactionId::EyrieDynasties) throw std::logic_error("Only the Eyrie builds roosts");
    if (!canPlace(faction, clearingId)) throw std::logic_error("Cannot place a roost at the Keep");
    auto& land = map_.clearing(clearingId);
    if (map_.ruler(clearingId) != faction || land.pieces(faction, PieceType::Building) > 0
        || land.usedBuildingSlots() + (land.hasRuin() ? 1 : 0) >= land.buildingSlots()
        || pieceCount(faction, PieceType::Building) >= 7) {
        throw std::logic_error("Build a roost in a ruled clearing with an open slot and no roost");
    }
    const auto cardIndex = requireDecree("Build", land.suit());
    land.add(faction, PieceType::Building);
    resolveDecree("Build", cardIndex);
    consumeAction();
}

void SingleGame::organize(FactionId faction, int clearingId) {
    requireAction(faction);
    if (faction != FactionId::WoodlandAlliance) throw std::logic_error("Only the Alliance organizes");
    if (!canPlace(faction, clearingId)) throw std::logic_error("Cannot place sympathy at the Keep");
    auto& land = map_.clearing(clearingId);
    if (land.pieces(faction, PieceType::Warrior) == 0
        || land.pieces(faction, PieceType::Token) > 0) {
        throw std::logic_error("Organize with an Alliance warrior in an unsympathetic clearing");
    }
    const int placed = sympathyPlaced();
    if (placed >= 10) throw std::logic_error("All sympathy tokens are already on the map");
    land.remove(faction, PieceType::Warrior);
    land.add(faction, PieceType::Token);
    awardPoints(faction, SympathyPoints.at(static_cast<std::size_t>(placed)));
    consumeAction();
}


std::size_t SingleGame::decreeColumnIndex(const std::string& column) {
    if (column == "Recruit") return 0;
    if (column == "Move") return 1;
    if (column == "Battle") return 2;
    if (column == "Build") return 3;
    throw std::invalid_argument("Unknown Decree column");
}

void SingleGame::chooseEyrieLeader(FactionId faction, const std::string& leader) {
    if (phase_ == Phase::Setup) requireSetupStep(faction, "choose_leader");
    else requireFactionPhase(faction, phase_ == Phase::Evening ? Phase::Evening : Phase::Birdsong);
    if (faction != FactionId::EyrieDynasties) {
        throw std::logic_error("Only the Eyrie chooses a leader");
    }
    auto& eyrie = player(faction).eyrie;
    if (phase_ == Phase::Evening && !eyrie.needsLeaderAfterTurmoil) {
        throw std::logic_error("Choose a replacement leader only after Turmoil");
    }
    if (!eyrie.leader.empty()) throw std::logic_error("The Eyrie leader is already chosen");
    if (std::find(eyrie.deposedLeaders.begin(), eyrie.deposedLeaders.end(), leader)
        != eyrie.deposedLeaders.end()) throw std::logic_error("That Eyrie leader has been deposed");

    std::array<std::string, 2> vizierColumns;
    if (leader == "Commander") vizierColumns = {"Move", "Battle"};
    else if (leader == "Despot") vizierColumns = {"Move", "Build"};
    else if (leader == "Builder") vizierColumns = {"Recruit", "Move"};
    else if (leader == "Charismatic") vizierColumns = {"Recruit", "Battle"};
    else throw std::invalid_argument("Unknown Eyrie leader");

    eyrie.leader = leader;
    eyrie.needsLeaderAfterTurmoil = false;
    for (const auto& column : vizierColumns) {
        eyrie.decree.at(decreeColumnIndex(column)).push_back({"Loyal Vizier", Suit::Bird});
    }
    if (phase_ == Phase::Setup) finishEyrieSetup();
}

void SingleGame::addToDecree(FactionId faction, std::size_t cardIndex,
                             const std::string& column) {
    requireFactionPhase(faction, Phase::Birdsong);
    if (faction != FactionId::EyrieDynasties) {
        throw std::logic_error("Only the Eyrie adds cards to the Decree");
    }
    auto& state = player(faction);
    if (state.eyrie.leader.empty()) throw std::logic_error("Choose an Eyrie leader first");
    if (state.eyrie.cardsAddedThisBirdsong >= 2) {
        throw std::logic_error("Only two cards can be added to the Decree each Birdsong");
    }
    if (cardIndex >= state.hand.size()) throw std::out_of_range("Unknown hand card");
    const auto& card = state.hand[cardIndex];
    if (state.eyrie.birdAddedThisBirdsong && card.getCardSuit() == Suit::Bird) {
        throw std::logic_error("Only one bird card may be added to the Decree each Birdsong");
    }

    state.eyrie.decree.at(decreeColumnIndex(column)).push_back(
        {card.getCardName(), card.getCardSuit(), false, card});
    state.eyrie.birdAddedThisBirdsong = state.eyrie.birdAddedThisBirdsong
        || card.getCardSuit() == Suit::Bird;
    ++state.eyrie.cardsAddedThisBirdsong;
    state.phaseStarted = true;
    state.hand.erase(state.hand.begin() + static_cast<std::ptrdiff_t>(cardIndex));
}

int SingleGame::decreeSize() const {
    int total = 0;
    for (const auto& column : player(FactionId::EyrieDynasties).eyrie.decree) {
        total += static_cast<int>(column.size());
    }
    return total;
}

std::string SingleGame::currentDecreeColumn() const {
    constexpr std::array<const char*, 4> names{"Recruit", "Move", "Battle", "Build"};
    const auto& decree = eyrieState().decree;
    for (std::size_t column = 0; column < decree.size(); ++column) {
        if (std::any_of(decree[column].begin(), decree[column].end(),
                        [](const auto& card) { return !card.resolved; })) return names[column];
    }
    return "";
}

std::size_t SingleGame::requireDecree(const std::string& column, Suit suit) const {
    if (currentDecreeColumn() != column) throw std::logic_error("Resolve Decree columns from left to right");
    const auto& cards = eyrieState().decree.at(decreeColumnIndex(column));
    // Use a matching suited card before a wild Vizier to preserve the player's options.
    for (const auto wanted : {suit, Suit::Bird}) {
        for (std::size_t index = 0; index < cards.size(); ++index) {
            if (!cards[index].resolved && cards[index].suit == wanted) return index;
        }
    }
    throw std::logic_error("This clearing does not match any unresolved Decree card");
}

void SingleGame::resolveDecree(const std::string& column, std::size_t cardIndex) {
    player(FactionId::EyrieDynasties).eyrie.decree.at(decreeColumnIndex(column)).at(cardIndex).resolved = true;
}

int SingleGame::pieceCount(FactionId faction, PieceType type) const {
    int total = 0;
    for (int id = 0; id < static_cast<int>(map_.size()); ++id) total += map_.clearing(id).pieces(faction, type);
    return total;
}

void SingleGame::enterEyrieEvening() {
    constexpr std::array<int, 8> points{0, 0, 1, 2, 3, 4, 4, 5};
    awardPoints(FactionId::EyrieDynasties, points.at(
        static_cast<std::size_t>(std::min(7, pieceCount(FactionId::EyrieDynasties, PieceType::Building)))));
}

ClearingOptions SingleGame::clearingOptions(FactionId faction, int clearingId) const {
    ClearingOptions options;
    if (winner_ || activeFaction() != faction || !decisions_.empty() || battle_) return options;
    const auto& land = map_.clearing(clearingId);
    const auto legal = [&](auto action) {
        try {
            auto preview = *this;
            action(preview);
            return true;
        } catch (const std::exception&) { return false; }
    };
    if (phase_ == Phase::Setup) {
        if (legal([&](auto& game) { game.placeSetupKeep(faction, clearingId); })) options.actions.push_back("place_keep");
        if (legal([&](auto& game) { game.placeSetupRoost(faction, clearingId); })) options.actions.push_back("place_roost");
        for (const auto type : {CatBuilding::Sawmill, CatBuilding::Workshop, CatBuilding::Recruiter}) {
            if (legal([&](auto& game) { game.placeSetupCatBuilding(faction, type, clearingId); })) {
                options.actions.push_back(type == CatBuilding::Sawmill ? "place_setup_sawmill"
                    : type == CatBuilding::Workshop ? "place_setup_workshop" : "place_setup_recruiter");
            }
        }
        return options;
    }
    for (const int target : map_.neighbors(clearingId)) {
        if (legal([&](auto& game) { game.move(faction, clearingId, target, 1); })) {
            options.moveTargets.push_back(target);
        }
        if (land.pieces(faction, PieceType::Warrior) > 0
            && legal([&](auto& game) { game.slip(faction, target); })) options.slipTargets.push_back(target);
    }
    if (!options.moveTargets.empty()) options.actions.push_back("move");
    if (!options.slipTargets.empty()) options.actions.push_back("slip");
    for (const auto& enemy : players_) {
        const auto target = enemy.faction.id();
        if (target != faction && legal([&](auto& game) { game.battle(faction, target, clearingId, 0, 0); })) {
            options.battleTargets.push_back(target);
        }
    }
    if (!options.battleTargets.empty()) options.actions.push_back("battle");
    if (faction != FactionId::MarquiseDeCat
        && legal([&](auto& game) { game.recruit(faction, clearingId); })) options.actions.push_back("recruit");
    for (const auto type : {CatBuilding::Sawmill, CatBuilding::Workshop, CatBuilding::Recruiter}) {
        if (legal([&](auto& game) { game.buildCatBuilding(faction, type, clearingId); })) {
            options.actions.push_back(type == CatBuilding::Sawmill ? "build_sawmill"
                : type == CatBuilding::Workshop ? "build_workshop" : "build_recruiter");
        }
    }
    for (std::size_t index = 0; index < hand(faction).size(); ++index) {
        if (legal([&](auto& game) { game.overwork(faction, clearingId, index); })) {
            options.actions.push_back("overwork");
            break;
        }
    }
    if (legal([&](auto& game) { game.buildRoost(faction, clearingId); })) options.actions.push_back("build_roost");
    if (legal([&](auto& game) { game.newRoost(faction, clearingId); })) options.actions.push_back("new_roost");
    if (legal([&](auto& game) { game.spreadSympathy(faction, clearingId); })) options.actions.push_back("spread_sympathy");
    if (legal([&](auto& game) { game.revolt(faction, clearingId); })) options.actions.push_back("revolt");
    if (legal([&](auto& game) { game.organize(faction, clearingId); })) options.actions.push_back("organize");
    if (legal([&](auto& game) { game.explore(faction, clearingId); })) options.actions.push_back("explore");
    if (land.pieces(faction, PieceType::Warrior) > 0
        && legal([&](auto& game) { game.repair(faction); })) options.actions.push_back("repair");
    for (const auto& enemy : players_) {
        if (legal([&](auto& game) { game.aid(faction, enemy.faction.id(), clearingId); })) {
            options.actions.push_back("aid");
            break;
        }
    }
    return options;
}

std::vector<std::string> SingleGame::phaseActions(FactionId faction) const {
    std::vector<std::string> actions;
    if (activeFaction() != faction || winner_ || !decisions_.empty() || battle_) return actions;
    const auto add = [&](const std::string& action) {
        if (std::find(actions.begin(), actions.end(), action) == actions.end()) actions.push_back(action);
    };
    const auto legal = [&](auto action) {
        try { auto preview = *this; action(preview); return true; }
        catch (const std::exception&) { return false; }
    };
    if (phase_ == Phase::Setup) {
        for (int id = 0; id < static_cast<int>(map_.size()); ++id) {
            for (const auto& action : clearingOptions(faction, id).actions) add(action);
        }
        if (setup_.step == "choose_leader") add("choose_eyrie_leader");
        return actions;
    }
    for (int id = 0; id < static_cast<int>(map_.size()); ++id) {
        for (const auto& action : clearingOptions(faction, id).actions) add(action);
    }
    if (legal([&](auto& game) { game.produceWood(faction); })) add("produce_wood");
    if (faction == FactionId::MarquiseDeCat && legal([&](auto& game) { game.recruit(faction, 0); })) add("recruit");
    if (legal([&](auto& game) { game.finishMarch(faction); })) add("finish_march");
    if (canDrawCard(faction)) add("draw_card");
    if (faction == FactionId::EyrieDynasties && eyrieState().leader.empty()) add("choose_eyrie_leader");
    for (std::size_t index = 0; index < hand(faction).size(); ++index) {
        for (const auto& action : cardActions(faction, index)) add(action);
    }
    return actions;
}

bool SingleGame::canAdvancePhase(FactionId faction) const {
    if (activeFaction() != faction || winner_) return false;
    try {
        auto preview = *this;
        preview.advancePhase();
        return true;
    } catch (const std::exception&) { return false; }
}
