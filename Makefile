CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -Werror -O2
PORT ?= 8080
WEB_ROOT ?= web
INCLUDES = -Ibasic
GAME_CORE = basic/game.cpp basic/rules.cpp basic/battle.cpp basic/vagabond.cpp basic/map/map.cpp basic/map/land.cpp basic/faction/faction.cpp \
	basic/card/share_card.cpp basic/card/share_deck.cpp
CORE = $(GAME_CORE) \
	basic/dice.cpp basic/item.cpp \
	basic/phase.cpp basic/effect/card_effect.cpp basic/faction/marquis_de_cat.cpp
SERVER = server/room_service.cpp server/http_server.cpp server/main.cpp
HEADERS = $(shell find basic server -name '*.hpp')

.PHONY: all test run clean

all: test build/mike-root_server

build/root_tests: $(CORE) $(HEADERS) tests/game_test.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CORE) tests/game_test.cpp -o $@

build/server_tests: $(GAME_CORE) $(HEADERS) server/room_service.cpp tests/server_test.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -pthread $(INCLUDES) $(GAME_CORE) server/room_service.cpp tests/server_test.cpp -o $@

build/mike-root_server: $(GAME_CORE) $(HEADERS) $(SERVER)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -pthread $(INCLUDES) $(GAME_CORE) $(SERVER) -o $@

build/rule_tests: $(GAME_CORE) $(HEADERS) tests/rules_test.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(GAME_CORE) tests/rules_test.cpp -o $@

test: build/root_tests build/server_tests build/rule_tests
	./build/root_tests
	./build/server_tests
	./build/rule_tests
	node tests/web_test.js

run: build/mike-root_server
	./build/mike-root_server $(PORT) $(WEB_ROOT)

clean:
	rm -rf build
