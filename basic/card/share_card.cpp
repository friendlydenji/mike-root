#include "share_card.hpp"

#include <array>
#include <stdexcept>
#include <type_traits>

namespace {
template <typename Enum, std::size_t Size>
std::string enumName(Enum value, const std::array<const char*, Size>& names) {
    const auto index = static_cast<std::size_t>(value);
    if (index >= names.size()) throw std::logic_error("Unknown craft card");
    return names[index];
}
}

ShareCard::ShareCard(Suit suit, CardEffectType effect,
                     std::optional<CraftEffect> craftingEffect,
                     std::optional<std::vector<Suit>> craftingIngredients)
    : cardSuit_(suit), effectType_(effect), craftingEffect_(std::move(craftingEffect)),
      craftingIngredients_(std::move(craftingIngredients)) {
    if ((effect == CardEffectType::ImmediateCrafting || effect == CardEffectType::PersistentCrafting)
        && (!craftingEffect_ || !craftingIngredients_)) {
        throw std::invalid_argument("Craft cards require an effect and ingredients");
    }
    cardNameAutoMapping();
}

void ShareCard::cardNameAutoMapping() {
    if (effectType_ == CardEffectType::Ambush || effectType_ == CardEffectType::Dominance) {
        const char* suit = cardSuit_ == Suit::Bird ? "Bird" : cardSuit_ == Suit::Fox ? "Fox"
            : cardSuit_ == Suit::Mouse ? "Mouse" : cardSuit_ == Suit::Rabbit ? "Rabbit" : "Any";
        name_ = std::string(suit) + (effectType_ == CardEffectType::Ambush ? " Ambush!" : " Dominance");
        return;
    }
    name_ = std::visit([](auto card) -> std::string {
        using Card = decltype(card);
        if constexpr (std::is_same_v<Card, BirdCraftCard>) {
            return enumName(card, std::array{"Woodland Runners", "Crossbow", "Birdy Bindle", "Arms Trader", "Armorers", "Royal Claim", "Brutal Tactics", "Sappers"});
        } else if constexpr (std::is_same_v<Card, FoxCraftCard>) {
            return enumName(card, std::array{"Favor of the Foxes", "Travel Gear", "Protection Racket", "Anvil", "Gently Used Knapsack", "Foxfolk Steel", "Root Tea", "Stand and Deliver", "Tax Collector"});
        } else if constexpr (std::is_same_v<Card, MouseCraftCard>) {
            return enumName(card, std::array{"Favor of the Mice", "Travel Gear", "Investments", "Crossbow", "Mouse in a Sack", "Sword", "Root Tea", "Codebreakers", "Scouting Party"});
        } else {
            return enumName(card, std::array{"Ambush", "Dominance", "Favor of the Rabbits", "A Visit to Friends", "Bake Sale", "Smuggler's Trail", "Root Tea", "Command Warren", "Cobbler", "Better Burrow Bank"});
        }
    }, craftingEffect_.value());
}
