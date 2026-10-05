CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -Werror -O2
INCLUDES = -Ibasic
GAME_CORE = basic/game.cpp basic/map/map.cpp basic/map/land.cpp basic/faction/faction.cpp \
	basic/card/share_card.cpp basic/card/share_deck.cpp
CORE = $(GAME_CORE) \
	basic/dice.cpp basic/item.cpp \
	basic/phase.cpp basic/effect/card_effect.cpp basic/faction/marquis_de_cat.cpp
SERVER = server/room_service.cpp server/http_server.cpp server/main.cpp

.PHONY: all test run clean

all: test build/gout_server

build/root_tests: $(CORE) tests/game_test.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CORE) tests/game_test.cpp -o $@

build/server_tests: $(GAME_CORE) server/room_service.cpp tests/server_test.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -pthread $(INCLUDES) $(GAME_CORE) server/room_service.cpp tests/server_test.cpp -o $@

build/gout_server: $(GAME_CORE) $(SERVER)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -pthread $(INCLUDES) $(GAME_CORE) $(SERVER) -o $@

test: build/root_tests build/server_tests
	./build/root_tests
	./build/server_tests

run: build/gout_server
	./build/gout_server 8080 web

clean:
	rm -rf build
