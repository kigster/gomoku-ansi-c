eing a great pair programmer.# vim: ft=make
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
PREFIX 		?= /usr/local
PACKAGE 	= gomoku
# directories
BINDIR 		= $(PREFIX)/bin
BINS 		= $(PACKAGE)

CC               = gcc
CXX              = g++
JSONC_DIR        = lib/json-c
JSONC_BUILD      = $(JSONC_DIR)/build
JSONC_LIB        = $(JSONC_BUILD)/libjson-c.a
CFLAGS           = -Wall -Wunused-parameter -Wextra -Wno-gnu-folding-constant -Wimplicit-function-declaration -Isrc -I$(JSONC_BUILD) -I$(JSONC_DIR) -O3
CXXFLAGS         = -Wall -Wunused-parameter -Wextra -std=c++17 -Isrc -Itests/googletest/googletest/include -I$(JSONC_BUILD) -I$(JSONC_DIR) -Wimplicit-function-declaration -O2
LDFLAGS          = -lm $(JSONC_LIB)

TARGET           = gomoku
SOURCES          = src/main.c src/gomoku.c src/board.c src/game.c src/ai.c src/ui.c src/cli.c
OBJECTS          = $(SOURCES:.c=.o)

# Test configuration
TEST_TARGET      = test_gomoku
TEST_SOURCES     = tests/gomoku_test.cpp src/gomoku.c src/board.c src/game.c src/ai.c
TEST_OBJECTS     = $(TEST_SOURCES:.cpp=.o)
TEST_OBJECTS    := $(TEST_OBJECTS:.c=.o)
GTEST_LIB        = tests/googletest/build/lib/libgtest.a
GTEST_MAIN_LIB   = tests/googletest/build/lib/libgtest_main.a

# CMake build directory
BUILD_DIR = build

.PHONY: clean test tag help cmake-build cmake-clean cmake-test install uninstall rebuild release json-c

help:		## Prints help message auto-generated from the comments.
		@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[32m%-20s\033[35m %s\033[0\n", $$1, $$2}' | sed '/^$$/d' | sort

version:        ## Prints the current version and tag
	        @echo "Version is $(VERSION)"
		@echo "The tag is $(TAG)"

build: 		$(TARGET) ## Build the Game

rebuild: 	clean build ## Clean and rebuild the game

json-c: 	$(JSONC_LIB)

$(JSONC_LIB):
		@if [ ! -f $(JSONC_LIB) ]; then \
			echo "Building json-c library..."; \
			mkdir -p $(JSONC_BUILD); \
			cd $(JSONC_BUILD) && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF >/dev/null 2>&1; \
			$(MAKE) -C $(JSONC_BUILD) -j4 >/dev/null 2>&1; \
		fi

$(TARGET): $(JSONC_LIB) $(OBJECTS)
		$(CC) $(OBJECTS) $(LDFLAGS) -o $(TARGET)

src/%.o: src/%.c
		$(CC) $(CFLAGS) -c $< -o $@

googletest: 	## Build GoogleTest framework (needed for running tests)
		@bash -c "./tests-setup >/dev/null"

$(TEST_TARGET): googletest $(JSONC_LIB) tests/gomoku_test.o src/gomoku.o src/board.o src/game.o src/ai.o # Test targets
		$(CXX) $(CXXFLAGS) tests/gomoku_test.o src/gomoku.o src/board.o src/game.o src/ai.o $(GTEST_LIB) $(GTEST_MAIN_LIB) $(JSONC_LIB) -pthread -o $(TEST_TARGET)

tests/gomoku_test.o: googletest tests/gomoku_test.cpp src/gomoku.h src/board.h src/game.h src/ai.h
		$(CXX) $(CXXFLAGS) -c tests/gomoku_test.cpp -o tests/gomoku_test.o

test: 		$(TEST_TARGET) $(TARGET) ## Run all the unit tests
		GREP_COLOR=32 ./$(TEST_TARGET) | grep --color=always -E 'GomokuTest\.([A-Za-z_]*)|tests|results|PASSED|FAILED'

tests: 		test

clean:  	## Clean up all the intermediate objects
		rm -f $(TARGET) $(TEST_TARGET) $(OBJECTS) tests/gomoku_test.o

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

install: build  ## Install the binary to the prefix
		@echo "Installing to $(PREFIX)"
		install -d $(BINDIR)
		install -m 755 $(BINS) $(BINDIR)

uninstall: 	## Uninstall the binary from the prefix
		-rm -f $(BINDIR)/$(PACKAGE)


format: 	## Format all source and test files using clang-format
		find src -maxdepth 2 -name '*.c**'   | xargs clang-format -i
		find tests -maxdepth 1 -name '*.c**' | xargs clang-format -i
