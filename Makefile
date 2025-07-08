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

CC               = gcc
CXX              = g++
CFLAGS           = -Wall -Wextra -Wimplicit-function-declaration -Isrc -O3
CXXFLAGS         = -Wall -Wextra -std=c++17 -Isrc -Itests/googletest/googletest/include -Wimplicit-function-declaration -O2
LDFLAGS          = -lm

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

.PHONY: clean test tag help

help:		## Prints help message auto-generated from the comments.
		@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[32m%-10s\033[35m %s\033[0\n", $$1, $$2}'

version:        ## Prints the current version and tag
	        @echo "Version is $(VERSION)"
		@echo "The tag is $(TAG)"

build: 		$(TARGET) ## Build the Game
 
rebuild: 	clean build ## Clean and rebuild the game

$(TARGET): $(OBJECTS)
		$(CC) $(OBJECTS) $(LDFLAGS) -o $(TARGET)

src/%.o: src/%.c
		$(CC) $(CFLAGS) -c $< -o $@

# Test targets
$(TEST_TARGET): tests/gomoku_test.o src/gomoku.o src/board.o src/game.o src/ai.o
		$(CXX) $(CXXFLAGS) tests/gomoku_test.o src/gomoku.o src/board.o src/game.o src/ai.o $(GTEST_LIB) $(GTEST_MAIN_LIB) -pthread -o $(TEST_TARGET)

tests/gomoku_test.o: tests/gomoku_test.cpp src/gomoku.h src/board.h src/game.h src/ai.h
		$(CXX) $(CXXFLAGS) -c tests/gomoku_test.cpp -o tests/gomoku_test.o

test: $(TEST_TARGET) $(TARGET) ## Run all the unit tests
		./$(TEST_TARGET)

clean:  	## Clean up all the intermediate objects
		rm -f $(TARGET) $(TEST_TARGET) $(OBJECTS) tests/gomoku_test.o

tag:    	## Tag the current git version with the tag equal to the VERSION constant
		git tag $(TAG) -f
		git push --tags -f

release:  	tag ## Update current VERSION tag to this SHA, and publish a new Github Release
		gh release create $(TAG) --generate-notes

