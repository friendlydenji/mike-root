# gout - Root game logic

A dependency-free C++17 rules engine and web multiplayer prototype for the
board game Root.

Implemented core rules:

- 12-clearing autumn map with roads, suits, slots, and ruins
- piece placement and clearing rule; Eyrie wins rule ties
- legal movement through adjacent clearings while ruling origin or destination
- battle dice, warrior hit limits, defenseless bonus, piece removal, and VP
- complete 54-card shared deck with suits, names, effects, and crafting costs
- private three-card starting hands with real PNP artwork and an Evening draw
- Birdsong, Daylight, Evening, three Daylight actions, player rotation
- 30-point victory and deterministic battle overloads for tests
- canonical opening placement for Marquise de Cat and Eyrie Dynasties

## Run locally

```sh
make test
make run
```

Open `http://127.0.0.1:8080` in two browser windows. Create a room in the first
window and join with its six-character code in the second.

The server is authoritative: clients submit actions, while C++ validates turns,
movement, battle, dice, scoring, and state revisions. Rooms are currently kept
in memory and support two players (Marquise de Cat and Eyrie Dynasties).

## Project layout

- `basic/`: reusable C++ game rules
- `server/`: dependency-free HTTP server and multiplayer room service
- `web/`: responsive browser client
- `assets/cards/`: extracted card fronts and their manifest
- `tests/`: game-rule and room-flow tests

The shared engine remains independent of faction boards. Decree,
turmoil, outrage, supporters, Vagabond items, crafting, dominance, hirelings,
and expansion factions can be implemented on top of this core without changing
movement, rule, battle, turn, or victory behavior.
