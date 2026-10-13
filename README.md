# mike-root - Root game logic

A dependency-free C++17 rules engine and web multiplayer prototype for the
board game Root.

Implemented core rules:

- 12-clearing Autumn map with the 18 printed paths, seven forests, suits,
  building slots, and four ruins aligned with the board artwork
- piece placement and clearing rule; Eyrie wins rule ties
- legal movement through adjacent clearings while ruling origin or destination
- battle dice, Ambush/counter-Ambush, crafted combat effects, player-chosen
  cardboard/item losses, warrior hit limits, defenseless bonus, and VP
- 54-card shared deck (50 cards in two-player games), shared discards and
  immediate reshuffling; discarded Dominance cards remain available to take
- private three-card starting hands with real PNP artwork and an Evening draw
- Birdsong, Daylight, Evening, faction-specific action budgets, player rotation
- 30-point victory and deterministic battle overloads for tests
- interactive standard setup: Cats choose a corner for the Keep and place their
  three starting buildings in legal slots; Eyrie place a roost and six warriors
  diagonally opposite the Keep and choose their starting leader before Birdsong
- Vagabond chooses Thief, Tinker, or Ranger and a starting forest during Setup
- Marquise Birdsong wood, typed building tracks, connected wood payments,
  Build scoring, once-per-turn Recruit, two-move March, Overwork and bird-card actions
- Keep placement restrictions and Field Hospitals, including losses outside battle
- item crafting with suit-matched crafting pieces, item supply and crafting order;
  Evening hand-limit discards
- Eyrie leader selection, real leader/Vizier artwork, Decree card placement,
  ordered suit-matched Recruit/Move/Battle/Build actions, leader combat/recruit
  bonuses, Turmoil and replacement leaders, Evening roost scoring
- A New Roost when the Eyrie has no roosts; Turmoil returns real Decree cards
  to the discard pile, never Loyal Viziers
- Woodland Alliance supporters, sympathy costs and scoring, martial law,
  revolt, mobilize, train, military operations, Guerrilla War, and Outrage
- Alliance Revolt-before-Spread order, ten-warrior/officer supply, base-loss
  penalties, supporter limit, and base-dependent Evening draws
- Vagabond per-item exhaustion/damage, chosen refresh and repair, once-per-turn
  Slip, forests and Rest, shuffled ruin items, Aid card/item choices and transfers,
  correct relationships/Infamy, Allied movement and battles, fifteen quests,
  Strike, character abilities, bags/tea/coins tracks and item capacity
- all base-deck persistent crafting effects, Favor cards, Dominance activation,
  alternative victory checks and four-player Vagabond coalitions

## Run locally

```sh
cd /home/tadienminhtri/pet-projects/mike-root
make test
make run
```

Open `http://127.0.0.1:8080` in two or more browser windows. Create a room in the
first window, join with its six-character code, then let the host start the game.
Players receive random factions only when the host starts.
Starting the room enters Setup, not Birdsong. Cats complete their placement
first, then Eyrie complete theirs, regardless of seating order. Starting placement
does not spend wood, actions, or score points. Once it is complete, the first
seated player starts Birdsong. If Cats are absent, Eyrie may choose any corner.
If a Vagabond is present, character and forest selection follow Eyrie setup.

To use another port:

```sh
make run PORT=8081
```

For LAN play, keep the server running and open `http://<server-ip>:8080` on the
other devices. The server listens on all network interfaces; the firewall must
allow inbound TCP traffic on the selected port.

The server is authoritative: clients submit actions, while C++ validates turns,
movement, battle, dice, scoring, card draws, and state revisions. Rooms are kept
in memory and support two to four players using the four base-game factions.
Choose a phase action before interacting with the map or spending a hand card.
Clearing highlights, available actions and legal movement destinations come
from the C++ validators. Cats produce wood before advancing from Birdsong.
Map action controls sit below the map so they do not
cover destination clearings on phones. Battle pauses for private reactions, then
the server rolls and broadcasts the same result to every player. Faction boards
can be flipped freely.
Battles pause for the appropriate player to answer Ambushes, use effects, choose
losses, or use Field Hospitals. Other actions remain blocked until responses
finish. Invalid actions are rolled back, and choices require the current revision.
Faction/card tools below the map expose valid quests, inventory choices, Aid,
character abilities, crafted effects, and Dominance. Forest markers are clickable.

## Project layout

- `basic/`: reusable C++ game rules
- `server/`: dependency-free HTTP server and multiplayer room service
- `web/`: responsive browser client
- `assets/cards/`: extracted card fronts and their manifest
- `tests/`: game-rule and room-flow tests

The shared engine remains independent of faction-board artwork. Expansion
factions, hirelings, alternate maps and advanced setup are outside this base-game
prototype. Rooms remain in memory; restarting the server resets them.

Rule reference: [Leder Games resources](https://ledergames.com/pages/resources).
The supplied PNP Alliance board predates the updated board. Sympathy scoring
intentionally matches its printed track: `0,1,1,1,2,2,3,4,4,4`. This is a legacy
board variant, not a claim of tournament parity with the latest printing.
