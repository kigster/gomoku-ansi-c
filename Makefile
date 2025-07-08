VERSION := "0.1.1"

CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -Wimplicit-function-declaration -Isrc -O2
CXXFLAGS = -Wall -Wextra -std=c++17 -Isrc -Itests/googletest/googletest/include -Wimplicit-function-declaration -O2
LDFLAGS = -lm

TARGET = gomoku
SOURCES = src/main.c src/gomoku.c src/board.c src/game.c src/ai.c src/ui.c src/cli.c
OBJECTS = $(SOURCES:.c=.o)

# Test configuration
TEST_TARGET = test_gomoku
TEST_SOURCES = tests/gomoku_test.cpp src/gomoku.c src/board.c src/game.c src/ai.c
TEST_OBJECTS = $(TEST_SOURCES:.cpp=.o)
TEST_OBJECTS := $(TEST_OBJECTS:.c=.o)
GTEST_LIB = tests/googletest/build/lib/libgtest.a
GTEST_MAIN_LIB = tests/googletest/build/lib/libgtest_main.a

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $(TARGET)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Test targets
$(TEST_TARGET): tests/gomoku_test.o src/gomoku.o src/board.o src/game.o src/ai.o
	$(CXX) $(CXXFLAGS) tests/gomoku_test.o src/gomoku.o src/board.o src/game.o src/ai.o $(GTEST_LIB) $(GTEST_MAIN_LIB) -pthread -o $(TEST_TARGET)

tests/gomoku_test.o: tests/gomoku_test.cpp src/gomoku.h src/board.h src/game.h src/ai.h
	$(CXX) $(CXXFLAGS) -c tests/gomoku_test.cpp -o tests/gomoku_test.o

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f $(TARGET) $(TEST_TARGET) $(OBJECTS) tests/gomoku_test.o

tag:
	git tag $(VERSION) -f
	git push --tags

.PHONY: clean test tag
