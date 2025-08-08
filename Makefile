# vim: ft=make
# vim: tabstop=8
# vim: shiftwidth=8
# vim: noexpandtab

OS              := $(shell uname -s | tr '[:upper:]' '[:lower:]')
MAKEFILE_PATH   := $(abspath $(lastword $(MAKEFILE_LIST)))
CURRENT_DIR     := $(shell ( cd .; pwd -P ) )
VERSION         := $(shell grep GAME_VERSION src/gomoku.hpp | head -1 | cut -d'"' -f2)
TAG             := $(shell echo "v$(VERSION)")
BRANCH          := $(shell git branch --show)

CC               = gcc
CXX              = g++
CFLAGS           = -Wall -Wunused-parameter -Wextra -Wimplicit-function-declaration -Isrc -O3
CXXFLAGS         = -Wall -Wunused-parameter -Wextra -Wpedantic -std=c++23 -Isrc -Itests/googletest/googletest/include -O3 -march=native
LDFLAGS          = -lm -pthread

TARGET           = gomoku
# Mixed C and C++ sources - convert gradually
CPP_SOURCES      = src/gomoku.cpp src/board.cpp src/main.cpp src/ui.cpp src/cli.cpp src/ai.cpp src/game.cpp
C_SOURCES        =
CPP_OBJECTS      = $(CPP_SOURCES:.cpp=.o)
C_OBJECTS        = $(C_SOURCES:.c=.o)
OBJECTS          = $(CPP_OBJECTS) $(C_OBJECTS)

# Test configuration
TEST_TARGET      = test_gomoku
TEST_CPP_SOURCES = tests/gomoku_test.cpp src/gomoku.cpp src/board.cpp
TEST_C_SOURCES   = src/game.c src/ai.c
TEST_CPP_OBJECTS = $(TEST_CPP_SOURCES:.cpp=.o)
TEST_C_OBJECTS   = $(TEST_C_SOURCES:.c=.o)
TEST_OBJECTS     = $(TEST_CPP_OBJECTS) $(TEST_C_OBJECTS)
GTEST_LIB        = tests/googletest/build/lib/libgtest.a
GTEST_MAIN_LIB   = tests/googletest/build/lib/libgtest_main.a

# CMake build directory
BUILD_DIR = build

.PHONY: clean test tag help cmake-build cmake-clean cmake-test

help:		## Prints help message auto-generated from the comments.
		@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[32m%-20s\033[35m %s\033[0\n", $$1, $$2}' | sed '/^$$/d' | sort

version:        ## Prints the current version and tag
	        @echo "Version is $(VERSION)"
		@echo "The tag is $(TAG)"

build: 		$(TARGET) ## Build the Game
 
rebuild: 	clean build ## Clean and rebuild the game

$(TARGET): $(OBJECTS)
		$(CXX) $(OBJECTS) $(LDFLAGS) -o $(TARGET)

# Compilation rules for C++ files
src/%.o: src/%.cpp
		$(CXX) $(CXXFLAGS) -c $< -o $@

# Compilation rules for C files  
src/%.o: src/%.c
		$(CC) $(CFLAGS) -c $< -o $@

$(TEST_TARGET): $(TEST_OBJECTS) # Test targets
		$(CXX) $(CXXFLAGS) $(TEST_OBJECTS) $(GTEST_LIB) $(GTEST_MAIN_LIB) -o $(TEST_TARGET)

tests/gomoku_test.o: tests/gomoku_test.cpp src/gomoku.hpp src/board.hpp src/game.h src/ai.h
		$(CXX) $(CXXFLAGS) -c tests/gomoku_test.cpp -o tests/gomoku_test.o

test: 		$(TEST_TARGET) $(TARGET) ## Run all the unit tests
		./$(TEST_TARGET)

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

