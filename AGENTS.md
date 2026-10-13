# Agent Guide

## Project Shape

- `basic/` is the authoritative C++17 game engine and rules layer.
- `server/` owns room state, validation, serialization, and the HTTP API.
- `web/` is a dependency-free browser client. It must render legal actions from the server rather than duplicate rule decisions.
- `tests/` contains engine, room/API, browser interaction, and full LAN regression tests.

## Required Workflow

Every feature change and bug fix must include a regression test in the same change. Choose the lowest layer that proves the behavior, and add browser or LAN coverage when the behavior crosses those boundaries.

- Rule changes: add or update a C++ test in `tests/game_test.cpp` or `tests/rules_test.cpp`.
- Room/API changes: add or update `tests/server_test.cpp`.
- Browser interaction changes: add or update `tests/web_test.js`.
- End-to-end multiplayer changes: extend `tests/qa_test.js` when a room flow or synchronization contract changes.

Run `make test` while iterating. Run `make qa` before handing off, committing, or pushing. Do not describe work as complete when either command fails; report the failing check and the remaining limitation.

## Behavioral Contracts

- The C++ server is authoritative. Clients submit intent and only enable actions returned by the server.
- Rejected actions must not mutate game state, consume action economy, or leave the browser interaction locked.
- Marquise March spends one action for up to two moves. Each move may carry any positive number of warriors available at its origin.
- Keep setup, starting buildings, Roost setup, leader selection, and other setup choices remain player decisions unless the rules explicitly make them automatic.
- Preserve the current base-faction scope and the documented PNP legacy sympathy-scoring variant unless a task explicitly changes them.

## Useful Commands

```sh
make test
make qa
make run PORT=8081
```

The LAN table is then available at `http://<host-ip>:8081`. Rooms are held in memory and reset when the server restarts.
