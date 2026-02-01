# vim: ft=make
# vim: tabstop=8
# vim: shiftwidth=8
# vim: noexpandtab

OS              := $(shell uname -s | tr '[:upper:]' '[:lower:]')
MAKEFILE_PATH   := $(abspath $(lastword $(MAKEFILE_LIST)))
CURRENT_DIR     := $(shell ( cd .; pwd -P ) )
VERSION         := $(shell grep VERSION src/gomoku.h | awk '{print $$3}' | tr -d '"')
TAG             := $(shell echo "v$(VERSION)")
BRANCH          := $(shell git branch --show)

# installation prefix (can override)
PREFIX 			?= /usr/local
PACKAGE 		= gomoku
DAEMON_PACKAGE  	= gomoku-httpd
DAEMON_CLIENT  		= gomoku-http-client
# directories	
BINDIR 			= $(PREFIX)/bin
BINS 			= $(PACKAGE) $(DAEMON_PACKAGE) $(DAEMON_CLIENT)

CC               	= gcc
CXX              	= g++
JSONC_DIR        	= lib/json-c
JSONC_BUILD      	= $(JSONC_DIR)/build
JSONC_LIB        	= $(JSONC_BUILD)/libjson-c.a
CFLAGS           	= -Wall -Wunused-parameter -Wextra -Wno-gnu-folding-constant -Wimplicit-function-declaration -Isrc -I$(JSONC_BUILD) -I$(JSONC_DIR) -O3
CXXFLAGS         	= -Wall -Wunused-parameter -Wextra -std=c++17 -Isrc -Itests/googletest/googletest/include -I$(JSONC_BUILD) -I$(JSONC_DIR) -Wimplicit-function-declaration -O2
LDFLAGS          	= -lm $(JSONC_LIB)

TARGET           	= $(PACKAGE)
SOURCES          	= src/main.c src/gomoku.c src/board.c src/game.c src/ai.c src/ui.c src/cli.c
OBJECTS          	= $(SOURCES:.c=.o)

# Daemon configuration
DAEMON_TARGET    	= $(DAEMON_PACKAGE)
DAEMON_CORE      	= src/gomoku.o src/board.o src/game.o src/ai.o
DAEMON_NET       	= src/net/main.o src/net/cli.o src/net/handlers.o src/net/json_api.o
LOGC_DIR         	= lib/log.c
LOGC_SRC         	= $(LOGC_DIR)/src/log.c
HTTPSERVER_DIR   	= lib/httpserver.h
# Platform-specific flags for httpserver.h
ifeq ($(OS),darwin)
HTTPSERVER_PLATFORM 	= -DKQUEUE
else
HTTPSERVER_PLATFORM	= -DEPOLL
endif
DAEMON_CFLAGS    	= $(CFLAGS) -I$(HTTPSERVER_DIR) -I$(HTTPSERVER_DIR)/src -I$(LOGC_DIR)/src $(HTTPSERVER_PLATFORM)

# Test HTTP client
DAEMON_CLIENT_TARGET 	= $(DAEMON_CLIENT)
TEST_HTTP_SRC    	= src/net/test_client.c

# Test configuration
TEST_TARGET     	= test_gomoku
TEST_SOURCES    	= tests/gomoku_test.cpp src/gomoku.c src/board.c src/game.c src/ai.c
TEST_OBJECTS    	= $(TEST_SOURCES:.cpp=.o)
GTEST_LIB       	= tests/googletest/build/lib/libgtest.a
GTEST_MAIN_LIB  	= tests/googletest/build/lib/libgtest_main.a

# Daemon test configuration
DAEMON_TEST_TARGET = test_daemon
DAEMON_TEST_CXXFLAGS = $(CXXFLAGS) -Ilib/log.c/src
LOGC_OBJ = lib/log.c/src/log.o

# CMake build directory
BUILD_DIR = build

.PHONY: all clean test tag help cmake-build cmake-clean cmake-test install uninstall rebuild release json-c gomoku-httpd test-daemon submodules-daemon test-gomoku-http test-client

help:		## Prints help message auto-generated from the comments.
		@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[32m%-20s\033[35m %s\033[0\n", $$1, $$2}' | sed '/^$$/d' | sort

version:        ## Prints the current version and tag
	        @echo "Version is $(VERSION)"
		@echo "The tag is $(TAG)"

all: 		submodules $(TARGET) $(DAEMON_TARGET) $(DAEMON_CLIENT_TARGET) ## Build both the game and HTTP daemon

rebuild: 	clean $(TARGET) ## Clean and rebuild the game

submodules: 	googletest ## Initialize and update git submodules
		@git submodule update --init --recursive $(JSONC_DIR) $(HTTPSERVER_DIR) $(LOGC_DIR)

json-c: 	$(JSONC_LIB)

$(JSONC_LIB): submodules
		@if [ ! -f $(JSONC_LIB) ]; then \
			echo "Building json-c library..."; \
			mkdir -p $(JSONC_BUILD); \
			LDFLAGS="" cmake -S $(JSONC_DIR) -B $(JSONC_BUILD) -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DBUILD_STATIC_LIBS=ON; \
			LDFLAGS="" $(MAKE) -C $(JSONC_BUILD) -j4; \
		fi

$(TARGET): $(JSONC_LIB) $(OBJECTS) ## Build the terminal game
		$(CC) $(OBJECTS) $(LDFLAGS) -o $(TARGET)

# Daemon targets (must come before generic src/%.o rule)
submodules-daemon: submodules ## Initialize daemon submodules (depends on submodules)

$(LOGC_OBJ): submodules-daemon
		$(CC) $(CFLAGS) -I$(LOGC_DIR)/src -c $(LOGC_SRC) -o $(LOGC_OBJ)

gomoku-httpd: submodules-daemon $(JSONC_LIB) $(DAEMON_CORE) $(DAEMON_NET) $(LOGC_OBJ) ## Build the HTTP daemon
		$(CC) $(DAEMON_CORE) $(DAEMON_NET) $(LOGC_OBJ) $(LDFLAGS) -o $(DAEMON_TARGET)

src/net/%.o: src/net/%.c | $(JSONC_LIB) submodules-daemon
		$(CC) $(DAEMON_CFLAGS) -c $< -o $@

# HTTP test client
$(DAEMON_CLIENT_TARGET): $(TEST_HTTP_SRC) ## Build HTTP test client
		$(CC) $(CFLAGS) $(TEST_HTTP_SRC) -o $(DAEMON_CLIENT_TARGET)

test-client: 	$(DAEMON_CLIENT_TARGET) ## Alias for building the HTTP test client

# Generic src rule (must come after src/net/%.o rule)
src/%.o: src/%.c | $(JSONC_LIB)
		$(CC) $(CFLAGS) -c $< -o $@

googletest: 	## Build GoogleTest framework (needed for running tests)
		@bash -c "./tests/tests-setup"

$(TEST_TARGET): googletest $(JSONC_LIB) tests/gomoku_test.o src/gomoku.o src/board.o src/game.o src/ai.o # Test targets
		$(CXX) $(CXXFLAGS) tests/gomoku_test.o src/gomoku.o src/board.o src/game.o src/ai.o $(GTEST_LIB) $(GTEST_MAIN_LIB) $(JSONC_LIB) -pthread -o $(TEST_TARGET)

tests/gomoku_test.o: googletest tests/gomoku_test.cpp src/gomoku.h src/board.h src/game.h src/ai.h
		$(CXX) $(CXXFLAGS) -c tests/gomoku_test.cpp -o tests/gomoku_test.o

test: 		$(TEST_TARGET) $(DAEMON_TEST_TARGET) $(TARGET) ## Run all unit tests (game + daemon)
		@echo "=== Running Game Tests ==="
		@GREP_COLOR=32 ./$(TEST_TARGET) | grep --color=always -E 'GomokuTest\.([A-Za-z_]*)|tests|results|PASSED|FAILED'
		@echo ""
		@echo "=== Running Daemon Tests ==="
		@GREP_COLOR=32 ./$(DAEMON_TEST_TARGET) | grep --color=always -E 'Daemon[A-Za-z]*Test\.([A-Za-z_]*)|tests|results|PASSED|FAILED'

tests: 		test

# Daemon tests
$(DAEMON_TEST_TARGET): googletest submodules-daemon $(JSONC_LIB) tests/daemon_test.o $(DAEMON_CORE) src/net/cli.o src/net/json_api.o $(LOGC_OBJ)
		$(CXX) tests/daemon_test.o $(DAEMON_CORE) src/net/cli.o src/net/json_api.o $(LOGC_OBJ) $(GTEST_LIB) $(GTEST_MAIN_LIB) $(JSONC_LIB) -pthread -o $(DAEMON_TEST_TARGET)

tests/daemon_test.o: googletest submodules-daemon tests/daemon_test.cpp src/net/cli.h src/net/json_api.h
		$(CXX) $(DAEMON_TEST_CXXFLAGS) -c tests/daemon_test.cpp -o tests/daemon_test.o

test-daemon: 	$(DAEMON_TEST_TARGET) ## Run daemon unit tests
		GREP_COLOR=32 ./$(DAEMON_TEST_TARGET) | grep --color=always -E 'Daemon[A-Za-z]*Test\.([A-Za-z_]*)|tests|results|PASSED|FAILED'

clean:  	## Clean up all the intermediate objects
		rm -f $(TARGET) $(TEST_TARGET) $(OBJECTS) tests/gomoku_test.o
		rm -f $(DAEMON_TARGET) $(DAEMON_TEST_TARGET) $(DAEMON_NET) tests/daemon_test.o $(LOGC_OBJ)
		rm -f $(DAEMON_CLIENT_TARGET)

tag:    	## Tag the current git version with the tag equal to the VERSION constant
		git tag $(TAG) -f
		git push --tags -f

release:  	tag ## Update current VERSION tag to this SHA, and publish a new Github Release
		gh release create $(TAG) --generate-notes

# CMake targets
cmake-build: 	## Build using CMake (creates build directory and runs cmake ..)
		mkdir -p $(BUILD_DIR)
		cd $(BUILD_DIR) && cmake .. && make

cmake-clean: 	## Clean CMake build directory
		rm -rf $(BUILD_DIR)

cmake-test: 	cmake-build ## Run tests using CMake
		cd $(BUILD_DIR) && ctest --verbose

cmake-rebuild: 	cmake-clean cmake-build ## Clean and rebuild using CMake

install: 	all ## Install the binaries to the prefix
		@echo "Installing to $(PREFIX)"
		install -d $(BINDIR)
		install -m 755 $(BINS) $(BINDIR)

uninstall: 	## Uninstall the binary from the prefix
		-rm -f $(BINDIR)/$(PACKAGE)

format: 	## Format all source and test files using clang-format
		find src -maxdepth 2 -name '*.c**'   | xargs clang-format -i
		find tests -maxdepth 1 -name '*.c**' | xargs clang-format -i
