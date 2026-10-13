#pragma once

#include <array>
#include <deque>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <utility>
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
    bool diceRolled{};
    int ambushHits{};
};

struct VagabondItems {
    int boots{2};
    int swords{1};
    int torches{1};
    int hammers{};
    int tea{};
    int exhaustedBoots{};
    int exhaustedSwords{};
    int exhaustedTorches{};
    int exhaustedHammers{};
    int exhaustedTea{};
    int damaged{};
};

struct VagabondItem {
    int id{};
    std::string type;
    bool exhausted{};
    bool damaged{};
};

struct RuleChoice {
    std::string value;
    std::string label;
};

struct RuleDecision {
    RuleDecision(FactionId actor, std::string type, std::string heading,
                 std::vector<RuleChoice> options, int location = -1, int count = 0,
                 Suit match = Suit::Any, FactionId enemy = FactionId::Count)
        : faction(actor), kind(std::move(type)), title(std::move(heading)), choices(std::move(options)),
          clearing(location), amount(count), suit(match), other(enemy) {}
    FactionId faction{};
    std::string kind;
    std::string title;
    std::vector<RuleChoice> choices;
    int clearing{-1};
    int amount{};
    Suit suit{Suit::Any};
    FactionId other{FactionId::Count};
};

struct RuleAction {
    RuleAction(std::string kind, std::string text, int handCard = -1, int itemId = -1,
               int location = -1, FactionId enemy = FactionId::Count, std::string selection = "")
        : type(std::move(kind)), label(std::move(text)), card(handCard), item(itemId),
          clearing(location), target(enemy), option(std::move(selection)) {}
    std::string type;
    std::string label;
    int card{-1};
    int item{-1};
    int clearing{-1};
    FactionId target{FactionId::Count};
    std::string option;
};

struct BattleState {
    unsigned id{};
    FactionId attacker{};
    FactionId defender{};
    int clearing{};
    int stage{};
    BattleResult result;
    bool ambushed{};
    int attackerExtra{};
    int defenderExtra{};
    bool attackerArmor{};
    bool defenderArmor{};
    FactionId ally{FactionId::Count};
    int alliedLosses{};
    int damagedItems{};
};

struct DecreeCard {
    DecreeCard(std::string name, Suit match, bool done = false, std::optional<ShareCard> card = std::nullopt)
        : title(std::move(name)), suit(match), resolved(done), original(std::move(card)) {}
    std::string title;
    Suit suit{Suit::Bird};
    bool resolved{};
    std::optional<ShareCard> original;
};

struct EyrieState {
    std::string leader;
    std::array<std::vector<DecreeCard>, 4> decree;
    int cardsAddedThisBirdsong{};
    bool birdAddedThisBirdsong{};
    std::vector<std::string> deposedLeaders;
    bool needsLeaderAfterTurmoil{};
};

struct ClearingOptions {
    std::vector<std::string> actions;
    std::vector<int> moveTargets;
    std::vector<int> slipTargets;
    std::vector<FactionId> battleTargets;
};

struct CatState {
    bool woodProduced{};
    bool recruited{};
    int marchMovesRemaining{};
    bool daylightActionsStarted{};
};

// Preset provides a fixed opening for isolated rule tests.
enum class SetupMode { Interactive, Preset };

struct SetupState {
    std::string step;
    std::optional<FactionId> faction;
    std::optional<int> keep;
    std::optional<int> eyrieCorner;
    std::array<bool, 3> buildingsPlaced{};
};

class SingleGame {
public:
    explicit SingleGame(std::vector<FactionId> factions, unsigned seed = std::random_device{}(),
                        SetupMode setupMode = SetupMode::Interactive);

    const Map& map() const noexcept { return map_; }
    Map& map() noexcept { return map_; }
    Phase phase() const noexcept { return phase_; }
    FactionId activeFaction() const;
    int actionsRemaining() const noexcept { return actionsRemaining_; }
    int score(FactionId faction) const;
    std::optional<FactionId> winner() const noexcept { return winner_; }
    const std::vector<ShareCard>& hand(FactionId faction) const;
    const std::vector<ShareCard>& supporters(FactionId faction) const;
    int officers(FactionId faction) const;
    int sympathyPlaced() const;
    int sympathyCost(int clearingId) const;
    const VagabondItems& vagabondItems() const;
    int relationship(FactionId faction) const;
    const EyrieState& eyrieState() const;
    const CatState& catState() const;
    const SetupState& setupState() const noexcept { return setup_; }
    int catBuildingCount(CatBuilding type) const;
    int catBuildCost(CatBuilding type) const;
    int catWoodCount() const;
    std::vector<std::string> phaseActions(FactionId faction) const;
    std::vector<std::string> cardActions(FactionId faction, std::size_t cardIndex) const;
    const std::map<std::string, int>& craftedItems(FactionId faction) const;
    std::string currentDecreeColumn() const;
    ClearingOptions clearingOptions(FactionId faction, int clearingId) const;
    bool canAdvancePhase(FactionId faction) const;
    std::size_t deckSize() const noexcept { return drawPile_.size(); }
    bool canDrawCard(FactionId faction) const noexcept;
    std::size_t discardSize() const noexcept { return discardPile_.size(); }
    const std::deque<RuleDecision>& decisions() const noexcept { return decisions_; }
    const std::optional<BattleState>& pendingBattle() const noexcept { return battle_; }
    const std::optional<BattleState>& completedBattle() const noexcept { return completedBattle_; }
    const std::vector<VagabondItem>& inventory() const noexcept { return inventory_; }
    int vagabondLocation() const;
    const std::string& vagabondCharacter() const noexcept { return character_; }
    const std::vector<ShareCard>& persistentCards(FactionId faction) const;
    std::optional<Suit> dominance(FactionId faction) const;
    std::optional<FactionId> coalition() const noexcept { return coalition_; }
    const std::vector<ShareCard>& availableDominance() const noexcept { return dominanceCards_; }
    const std::vector<ShareCard>& discardPile() const noexcept { return discardPile_; }
    const std::vector<RuleChoice>& quests() const noexcept { return quests_; }
    std::vector<RuleAction> ruleActions(FactionId faction) const;
    void executeRuleAction(FactionId faction, std::size_t index);
    void respond(FactionId faction, const std::string& choice);
    void beginBattle(FactionId attacker, FactionId defender, int clearing,
                     int firstRoll = -1, int secondRoll = -1);
    void newRoost(FactionId faction, int clearing);
    static const std::array<std::vector<int>, 7>& forests();
    bool vagabondAdjacent(int from, int to) const;

    void reset();
    void advancePhase();
    void drawCard(FactionId faction);
    void move(FactionId faction, int from, int to, int warriors);
    BattleResult battle(FactionId attacker, FactionId defender, int clearingId);
    BattleResult battle(FactionId attacker, FactionId defender, int clearingId,
                        int firstRoll, int secondRoll);
    void placePiece(FactionId faction, PieceType type, int clearingId, int amount = 1);
    void removePiece(FactionId faction, PieceType type, int clearingId, int amount = 1);
    void awardPoints(FactionId faction, int points);
    void spreadSympathy(FactionId faction, int clearingId);
    void revolt(FactionId faction, int clearingId);
    void mobilize(FactionId faction, std::size_t cardIndex);
    void train(FactionId faction, std::size_t cardIndex);
    void recruit(FactionId faction, int clearingId);
    void buildRoost(FactionId faction, int clearingId);
    void organize(FactionId faction, int clearingId);
    void slip(FactionId faction, int to);
    std::string explore(FactionId faction, int clearingId);
    void aid(FactionId faction, FactionId target, int clearingId);
    void repair(FactionId faction);
    void chooseEyrieLeader(FactionId faction, const std::string& leader);
    void addToDecree(FactionId faction, std::size_t cardIndex, const std::string& column);
    void produceWood(FactionId faction);
    void buildCatBuilding(FactionId faction, CatBuilding type, int clearingId);
    void overwork(FactionId faction, int clearingId, std::size_t cardIndex);
    void spendBirdCard(FactionId faction, std::size_t cardIndex);
    void finishMarch(FactionId faction);
    void discardCard(FactionId faction, std::size_t cardIndex);
    void craft(FactionId faction, std::size_t cardIndex);
    void placeSetupKeep(FactionId faction, int clearingId);
    void placeSetupCatBuilding(FactionId faction, CatBuilding type, int clearingId);
    void placeSetupRoost(FactionId faction, int clearingId);

private:
    friend struct GameTestAccess;
    struct PlayerState {
        explicit PlayerState(FactionId id) : faction(id) {}
        Faction faction;
        int score{};
        std::vector<ShareCard> hand;
        std::vector<ShareCard> supporters;
        int officers{};
        VagabondItems vagabond;
        EyrieState eyrie;
        CatState cat;
        std::map<std::string, int> craftedItems;
        std::array<int, 12> craftingUsed{};
        std::array<int, factionIndex(FactionId::Count)> relationships{};
        std::vector<ShareCard> persistent;
        std::vector<std::string> effectsUsed;
        std::optional<Suit> dominance;
        bool spreadStarted{};
        bool allianceDaylightStarted{};
        bool phaseStarted{};
        std::array<int, factionIndex(FactionId::Count)> aids{};
    };

    Map map_;
    std::vector<PlayerState> players_;
    std::size_t activePlayer_{};
    Phase phase_{Phase::Birdsong};
    int actionsRemaining_{};
    bool eveningCardDrawn_{};
    std::optional<FactionId> winner_;
    std::mt19937 random_;
    std::vector<ShareCard> drawPile_;
    std::map<std::string, int> itemSupply_;
    SetupMode setupMode_;
    SetupState setup_;
    std::vector<ShareCard> discardPile_;
    std::vector<ShareCard> dominanceCards_;
    std::deque<RuleDecision> decisions_;
    std::optional<BattleState> battle_;
    std::optional<BattleState> completedBattle_;
    std::vector<VagabondItem> inventory_;
    std::map<int, std::string> ruinItems_;
    std::string character_;
    int forest_{-1};
    bool slipped_{};
    int refreshes_{};
    int nextItemId_{};
    std::optional<FactionId> coalition_;
    bool effectAction_{};
    unsigned battleId_{};
    std::vector<RuleChoice> quests_;
    std::vector<int> questDeck_;
    std::array<int, 3> completedQuests_{};

    PlayerState& player(FactionId faction);
    const PlayerState& player(FactionId faction) const;
    void requireAction(FactionId faction) const;
    void consumeAction();
    void setupFactions();
    bool hasFaction(FactionId faction) const;
    void requireSetupStep(FactionId faction, const std::string& step) const;
    void beginEyrieSetup();
    void finishSetup();
    void drawCards(PlayerState& player, int amount);
    void requireFactionPhase(FactionId faction, Phase phase) const;
    bool spendSupporters(PlayerState& state, Suit suit, int amount);
    void gainSupporter(PlayerState& state, ShareCard card);
    void triggerOutrage(FactionId offender, Suit suit);
    void refreshVagabond();
    int vagabondItemCount() const;
    static std::size_t decreeColumnIndex(const std::string& column);
    int decreeSize() const;
    std::size_t requireDecree(const std::string& column, Suit suit) const;
    void resolveDecree(const std::string& column, std::size_t cardIndex);
    int pieceCount(FactionId faction, PieceType type) const;
    void enterEyrieEvening();
    std::vector<int> connectedCatClearings(int clearingId) const;
    void discard(ShareCard card);
    void discardHand(PlayerState& state, std::size_t index);
    void recycleDeck();
    bool canPlace(FactionId faction, int clearing) const;
    int allianceSupply() const;
    void baseLost(Suit suit);
    void hospitals(int clearing, int warriors);
    void queueLosses(FactionId faction, int clearing, int hits, FactionId remover);
    std::vector<RuleChoice> lossChoices(FactionId faction, int clearing) const;
    void removeChosenPiece(FactionId faction, int clearing, const std::string& piece, FactionId remover);
    void progressBattle();
    void refreshDecisions();
    void validateBattle(FactionId attacker, FactionId defender, int clearing) const;
    bool hasEffect(FactionId faction, const std::string& name) const;
    void initializeVagabond();
    void addItem(const std::string& type);
    void syncItems();
    int readyItems(const std::string& type) const;
    int undamagedItems(const std::string& type) const;
    void exhaustItem(const std::string& type, int amount = 1);
    VagabondItem& itemById(int id);
    bool onTrack(const VagabondItem& item) const;
    int satchelSize() const;
    int itemCapacity() const;
    void restVagabond();
    void aidChosen(FactionId faction, FactionId target, int clearing, int card, int item, const std::string& take);
    void strike(FactionId faction, FactionId target, int clearing, const std::string& piece);
    void useEffect(FactionId faction, int index, FactionId target, int clearing);
    void activateDominance(FactionId faction, std::size_t card, FactionId target);
    void applyRuleAction(FactionId faction, const RuleAction& action);
    void checkDominance();
    void finishEyrieSetup();
};
