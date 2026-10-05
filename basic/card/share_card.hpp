#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "../suit.hpp"

class ShareCard {
public:
    enum class CardEffectType { Ambush, Dominance, ImmediateCrafting, PersistentCrafting };
    enum class BirdCraftCard { WoodlandRunners, Crossbow, BirdyBindle, ArmsTrader, Armorers, RoyalClaim, BrutalTactics, Sappers };
    enum class FoxCraftCard { FavoroftheFoxes, TravelGear, ProtectionRacket, Anvil, GentlyUsedKnapsack, FoxfolkSteel, RootTea, StandandDeliver, TaxCollector };
    enum class MouseCraftCard { FavoroftheMice, TravelGear, Investments, Crossbow, Mouse_in_a_Sack, Sword, RootTea, Codebreakers, ScoutingParty };
    enum class RabbitCraftCard { Ambush, Dominance, FavoroftheRabbit, AVisittoFriends, BakeSale, SmugglersTrail, RootTea, CommandWarren, Cobbler, BetterBurrowBank };
    using CraftEffect = std::variant<BirdCraftCard, FoxCraftCard, MouseCraftCard, RabbitCraftCard>;

    ShareCard(Suit suit, CardEffectType effect,
              std::optional<CraftEffect> craftingEffect = std::nullopt,
              std::optional<std::vector<Suit>> craftingIngredients = std::nullopt);

    Suit getCardSuit() const noexcept { return cardSuit_; }
    CardEffectType getEffectType() const noexcept { return effectType_; }
    const std::string& getCardName() const noexcept { return name_; }
    const std::optional<CraftEffect>& getCraftingEffect() const noexcept { return craftingEffect_; }
    const std::optional<std::vector<Suit>>& getCraftingIngredients() const noexcept { return craftingIngredients_; }

private:
    Suit cardSuit_;
    CardEffectType effectType_;
    std::optional<CraftEffect> craftingEffect_;
    std::optional<std::vector<Suit>> craftingIngredients_;
    std::string name_;
    void cardNameAutoMapping();
};
