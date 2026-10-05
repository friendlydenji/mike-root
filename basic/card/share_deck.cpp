#include "share_deck.hpp"

#include <iostream>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace {
std::vector<ShareCard> shareDeck;
std::mutex shareDeckMutex;

void addCard(Suit suit, ShareCard::CardEffectType effect, std::nullopt_t, std::nullopt_t)
{
    shareDeck.emplace_back(suit, effect);
}

template <typename CraftCard>
void addCard(Suit suit, ShareCard::CardEffectType effect, CraftCard card,
             std::initializer_list<Suit> ingredients)
{
    shareDeck.emplace_back(suit, effect, ShareCard::CraftEffect{card},
                           std::vector<Suit>{ingredients});
}

void shareBirdCardCreate()
{
    /* bird ambush */
    addCard(Suit::Bird, ShareCard::CardEffectType::Ambush, std::nullopt, std::nullopt);
    addCard(Suit::Bird, ShareCard::CardEffectType::Ambush, std::nullopt, std::nullopt);
    /* bird dominance */
    addCard(Suit::Bird, ShareCard::CardEffectType::Dominance, std::nullopt, std::nullopt);
    /* bird immediate crafting */
    addCard(Suit::Bird, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::BirdCraftCard::WoodlandRunners,      {Suit::Rabbit});
    addCard(Suit::Bird, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::BirdCraftCard::Crossbow,             {Suit::Fox});
    addCard(Suit::Bird, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::BirdCraftCard::BirdyBindle,          {Suit::Mouse});
    addCard(Suit::Bird, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::BirdCraftCard::ArmsTrader,           {Suit::Fox, Suit::Fox});
    /* bird persistent crafting */
    addCard(Suit::Bird, ShareCard::CardEffectType::PersistentCrafting,
        ShareCard::BirdCraftCard::Armorers,             {Suit::Fox});
    addCard(Suit::Bird, ShareCard::CardEffectType::PersistentCrafting,
        ShareCard::BirdCraftCard::Armorers,             {Suit::Fox});
    addCard(Suit::Bird, ShareCard::CardEffectType::PersistentCrafting,
        ShareCard::BirdCraftCard::RoyalClaim,           {Suit::Any, Suit::Any, Suit::Any, Suit::Any});
    addCard(Suit::Bird, ShareCard::CardEffectType::PersistentCrafting,
        ShareCard::BirdCraftCard::BrutalTactics,        {Suit::Fox, Suit::Fox});
    addCard(Suit::Bird, ShareCard::CardEffectType::PersistentCrafting,
        ShareCard::BirdCraftCard::BrutalTactics,        {Suit::Fox, Suit::Fox});
    addCard(Suit::Bird, ShareCard::CardEffectType::PersistentCrafting,
        ShareCard::BirdCraftCard::Sappers,              {Suit::Mouse});
    addCard(Suit::Bird, ShareCard::CardEffectType::PersistentCrafting,
        ShareCard::BirdCraftCard::Sappers,              {Suit::Mouse});
}

void shareFoxCardCreate()
{
    /* fox ambush */
    addCard(Suit::Fox, ShareCard::CardEffectType::Ambush, std::nullopt, std::nullopt);
    /* fox dominance */
    addCard(Suit::Fox, ShareCard::CardEffectType::Dominance, std::nullopt, std::nullopt);
    /* fox immediate crafting */
    addCard(Suit::Fox, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::FoxCraftCard::FavoroftheFoxes,       {Suit::Fox, Suit::Fox, Suit::Fox});
    addCard(Suit::Fox, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::FoxCraftCard::TravelGear,            {Suit::Rabbit});
    addCard(Suit::Fox, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::FoxCraftCard::ProtectionRacket,      {Suit::Rabbit, Suit::Rabbit});
    addCard(Suit::Fox, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::FoxCraftCard::Anvil,                 {Suit::Fox});
    addCard(Suit::Fox, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::FoxCraftCard::GentlyUsedKnapsack,    {Suit::Mouse});
    addCard(Suit::Fox, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::FoxCraftCard::FoxfolkSteel,          {Suit::Fox, Suit::Fox});
    addCard(Suit::Fox, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::FoxCraftCard::RootTea,               {Suit::Mouse});
    /* fox persistent crafting */
    addCard(Suit::Fox, ShareCard::CardEffectType::PersistentCrafting,
        ShareCard::FoxCraftCard::StandandDeliver,       {Suit::Mouse, Suit::Mouse, Suit::Mouse});
    addCard(Suit::Fox, ShareCard::CardEffectType::PersistentCrafting,
        ShareCard::FoxCraftCard::StandandDeliver,       {Suit::Mouse, Suit::Mouse, Suit::Mouse});
    addCard(Suit::Fox, ShareCard::CardEffectType::PersistentCrafting,
        ShareCard::FoxCraftCard::TaxCollector,          {Suit::Rabbit, Suit::Fox, Suit::Mouse});
    addCard(Suit::Fox, ShareCard::CardEffectType::PersistentCrafting,
        ShareCard::FoxCraftCard::TaxCollector,          {Suit::Rabbit, Suit::Fox, Suit::Mouse});
    addCard(Suit::Fox, ShareCard::CardEffectType::PersistentCrafting,
        ShareCard::FoxCraftCard::TaxCollector,          {Suit::Rabbit, Suit::Fox, Suit::Mouse});
}

void shareMouseCardCreate()
{
    /* mouse ambush */
    addCard(Suit::Mouse, ShareCard::CardEffectType::Ambush, std::nullopt, std::nullopt);
    /* mouse dominance */
    addCard(Suit::Mouse, ShareCard::CardEffectType::Dominance, std::nullopt, std::nullopt);
    /* mouse immediate crafting */
    addCard(Suit::Mouse, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::MouseCraftCard::FavoroftheMice,      {Suit::Mouse, Suit::Mouse, Suit::Mouse});
    addCard(Suit::Mouse, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::MouseCraftCard::TravelGear,          {Suit::Rabbit});
    addCard(Suit::Mouse, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::MouseCraftCard::Investments,         {Suit::Rabbit, Suit::Rabbit});
    addCard(Suit::Mouse, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::MouseCraftCard::Crossbow,            {Suit::Fox});
    addCard(Suit::Mouse, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::MouseCraftCard::Mouse_in_a_Sack,     {Suit::Mouse});
    addCard(Suit::Mouse, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::MouseCraftCard::Sword,               {Suit::Fox, Suit::Fox});
    addCard(Suit::Mouse, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::MouseCraftCard::RootTea,             {Suit::Mouse});
    /* mouse persistent crafting */
    addCard(Suit::Mouse, ShareCard::CardEffectType::PersistentCrafting,
        ShareCard::MouseCraftCard::Codebreakers,        {Suit::Mouse});
    addCard(Suit::Mouse, ShareCard::CardEffectType::PersistentCrafting,
        ShareCard::MouseCraftCard::Codebreakers,        {Suit::Mouse});
    addCard(Suit::Mouse, ShareCard::CardEffectType::PersistentCrafting,
        ShareCard::MouseCraftCard::ScoutingParty,       {Suit::Mouse, Suit::Mouse});
    addCard(Suit::Mouse, ShareCard::CardEffectType::PersistentCrafting,
        ShareCard::MouseCraftCard::ScoutingParty,       {Suit::Mouse, Suit::Mouse});

}

void shareRabbitCardCreate()
{
    /* rabbit ambush */
    addCard(Suit::Rabbit, ShareCard::CardEffectType::Ambush, std::nullopt, std::nullopt);
    /* rabbit dominance */
    addCard(Suit::Rabbit, ShareCard::CardEffectType::Dominance, std::nullopt, std::nullopt);
    /* rabbit immediate crafting */
    addCard(Suit::Rabbit, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::RabbitCraftCard::FavoroftheRabbit,   {Suit::Rabbit, Suit::Rabbit, Suit::Rabbit});
    addCard(Suit::Rabbit, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::RabbitCraftCard::AVisittoFriends,    {Suit::Rabbit});
    addCard(Suit::Rabbit, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::RabbitCraftCard::BakeSale,           {Suit::Rabbit, Suit::Rabbit});
    addCard(Suit::Rabbit, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::RabbitCraftCard::SmugglersTrail,     {Suit::Mouse});
    addCard(Suit::Rabbit, ShareCard::CardEffectType::ImmediateCrafting,
        ShareCard::RabbitCraftCard::RootTea,            {Suit::Mouse});
    /* rabbit persistent crafting */
    addCard(Suit::Rabbit, ShareCard::CardEffectType::PersistentCrafting,
        ShareCard::RabbitCraftCard::CommandWarren,      {Suit::Rabbit, Suit::Rabbit});
    addCard(Suit::Rabbit, ShareCard::CardEffectType::PersistentCrafting,
        ShareCard::RabbitCraftCard::CommandWarren,      {Suit::Rabbit, Suit::Rabbit});
    addCard(Suit::Rabbit, ShareCard::CardEffectType::PersistentCrafting,
        ShareCard::RabbitCraftCard::Cobbler,            {Suit::Rabbit, Suit::Rabbit});
    addCard(Suit::Rabbit, ShareCard::CardEffectType::PersistentCrafting,
        ShareCard::RabbitCraftCard::Cobbler,            {Suit::Rabbit, Suit::Rabbit});
    addCard(Suit::Rabbit, ShareCard::CardEffectType::PersistentCrafting,
        ShareCard::RabbitCraftCard::BetterBurrowBank,   {Suit::Rabbit, Suit::Rabbit});
    addCard(Suit::Rabbit, ShareCard::CardEffectType::PersistentCrafting,
        ShareCard::RabbitCraftCard::BetterBurrowBank,   {Suit::Rabbit, Suit::Rabbit});
}

void shareDeckCreate()
{
    shareDeck.clear();
    shareBirdCardCreate();
    shareFoxCardCreate();
    shareMouseCardCreate();
    shareRabbitCardCreate();

    if (shareDeck.size() != 54)
    {
        throw std::runtime_error("Error: Share Deck size is not 54. Actual size: " + std::to_string(shareDeck.size()));
    }
}
}

std::vector<ShareCard> createShareDeck()
{
    std::lock_guard lock(shareDeckMutex);
    shareDeckCreate();
    return shareDeck;
}
