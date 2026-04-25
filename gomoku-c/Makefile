# vim: ft=make
# vim: tabstop=8
# vim: shiftwidth=8
# vim: noexpandtab
#
# This Makefile handles C/C++ compilation only.
# For all other tasks (docker, deploy, evals, formatting, etc.) use: just --list

OS              := $(shell uname -s | tr '[:upper:]' '[:lower:]')

CC               	= gcc
CXX              	= g++
JSONC_DIR        	= src/vendor/json-c
JSONC_BUILD      	= $(JSONC_DIR)/build
JSONC_LIB        	= $(JSONC_BUILD)/libjson-c.a
CFLAGS           	= -Wall -Wunused-parameter -Wextra -Wno-gnu-folding-constant -Wimplicit-function-declaration -Wno-sign-compare -Isrc/gomoku -Isrc -I$(JSONC_BUILD) -I$(JSONC_DIR) -O3
# On macOS, Apple Clang may use an incomplete C++ include dir and miss standard headers
# (e.g. cstddef, cstdlib). Add the SDK's C++ include path when available.
MACOS_CXX_INCLUDE	= $(shell [ "$(OS)" = darwin ] && sdk=$$(xcrun --show-sdk-path 2>/dev/null) && [ -d "$$sdk/usr/include/c++/v1" ] && echo "-I$$sdk/usr/include/c++/v1")
_CXXFLAGS        	= -Wall -Wunused-parameter -Wextra -std=c++17 -Isrc/gomoku -Isrc -Itests/googletest/googletest/include -I$(JSONC_BUILD) -I$(JSONC_DIR) -Wimplicit-function-declaration -O2 $(MACOS_CXX_INCLUDE)
ALL_CXXFLAGS     	= $(_CXXFLAGS) $(CXXFLAGS)
LDFLAGS          	= -lm $(JSONC_LIB)

PACKAGE 		= gomoku
DAEMON_PACKAGE  	= gomoku-httpd
DAEMON_CLIENT  		= gomoku-http-client
BINDIR           	= bin

TARGET           	= $(BINDIR)/$(PACKAGE)
SOURCES          	= src/gomoku/main.c src/gomoku/gomoku.c src/gomoku/board.c src/gomoku/game.c src/gomoku/ai.c src/gomoku/ui.c src/gomoku/cli.c
OBJECTS          	= $(SOURCES:.c=.o)

# Daemon configuration
DAEMON_TARGET    	= $(BINDIR)/$(DAEMON_PACKAGE)
DAEMON_CORE      	= src/gomoku/gomoku.o src/gomoku/board.o src/gomoku/game.o src/gomoku/ai.o
DAEMON_NET       	= src/net/main.o src/net/cli.o src/net/handlers.o src/net/json_api.o src/net/logger.o
HTTPSERVER_DIR   	= src/vendor/httpserver
# Platform-specific flags for httpserver.h
ifeq ($(OS),darwin)
HTTPSERVER_PLATFORM 	= -DKQUEUE
else
HTTPSERVER_PLATFORM	= -DEPOLL
endif
DAEMON_CFLAGS    	= $(CFLAGS) -I$(HTTPSERVER_DIR) $(HTTPSERVER_PLATFORM)

# Test HTTP client
DAEMON_CLIENT_TARGET 	= $(BINDIR)/$(DAEMON_CLIENT)
TEST_HTTP_SRC    	= src/net/test_client.c src/net/test_client_utils.c

# Test configuration
TEST_TARGET     	= $(BINDIR)/test_gomoku
TEST_SOURCES    	= tests/gomoku_test.cpp src/gomoku.c src/board.c src/game.c src/ai.c
TEST_OBJECTS    	= $(TEST_SOURCES:.cpp=.o)
GTEST_LIB       	= tests/googletest/build/lib/libgtest.a
GTEST_MAIN_LIB  	= tests/googletest/build/lib/libgtest_main.a

# Daemon test configuration
DAEMON_TEST_TARGET = $(BINDIR)/test_daemon
DAEMON_TEST_CXXFLAGS = $(ALL_CXXFLAGS)

# CMake build directory
BUILD_DIR = build

.PHONY: all clean test tests rebuild json-c gomoku-httpd googletest test-client test-daemon cmake-clean

$(BINDIR):
		@mkdir -p $(BINDIR)

all: 		$(TARGET) $(DAEMON_PACKAGE) $(DAEMON_CLIENT_TARGET)

install: 	all
		cp -pv $(TARGET) $(DAEMON_TARGET) $(DAEMON_CLIENT_TARGET) ../bin

docker-install: all
		cp -pv $(TARGET) $(DAEMON_TARGET) $(DAEMON_CLIENT_TARGET) ./bin


rebuild: 	clean all install

json-c: 	$(JSONC_LIB)

$(JSONC_LIB):
		@if [ ! -f $(JSONC_LIB) ]; then \
			echo "Building json-c library..."; \
			mkdir -p $(JSONC_BUILD); \
			LDFLAGS="" cmake -S $(JSONC_DIR) -B $(JSONC_BUILD) -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DBUILD_STATIC_LIBS=ON; \
			LDFLAGS="" $(MAKE) -C $(JSONC_BUILD) -j4; \
		fi

$(TARGET): $(JSONC_LIB) $(OBJECTS) | $(BINDIR)
		$(CC) $(OBJECTS) $(LDFLAGS) -o $(TARGET)

# Daemon targets (must come before generic src/%.o rule)
gomoku-httpd: $(JSONC_LIB) $(DAEMON_CORE) $(DAEMON_NET) | $(BINDIR)
		$(CC) $(DAEMON_CORE) $(DAEMON_NET) $(LDFLAGS) -lpthread -o $(DAEMON_TARGET)

src/net/%.o: src/net/%.c | $(JSONC_LIB)
		$(CC) $(DAEMON_CFLAGS) -c $< -o $@

# HTTP test client
$(DAEMON_CLIENT_TARGET): $(TEST_HTTP_SRC) | $(BINDIR)
		$(CC) $(CFLAGS) $(TEST_HTTP_SRC) -o $(DAEMON_CLIENT_TARGET)

test-client: 	$(DAEMON_CLIENT_TARGET)

# Generic src rules (net rule must come before gomoku rule)
src/gomoku/%.o: src/gomoku/%.c | $(JSONC_LIB)
		$(CC) $(CFLAGS) -c $< -o $@

googletest:
		@bash -c "./tests/tests-setup"

$(TEST_TARGET): googletest $(JSONC_LIB) tests/gomoku_test.o src/gomoku/gomoku.o src/gomoku/board.o src/gomoku/game.o src/gomoku/ai.o | $(BINDIR)
		$(CXX) $(ALL_CXXFLAGS) tests/gomoku_test.o src/gomoku/gomoku.o src/gomoku/board.o src/gomoku/game.o src/gomoku/ai.o $(GTEST_LIB) $(GTEST_MAIN_LIB) $(JSONC_LIB) -pthread -o $(TEST_TARGET)

tests/gomoku_test.o: googletest tests/gomoku_test.cpp src/gomoku/gomoku.h src/gomoku/board.h src/gomoku/game.h src/gomoku/ai.h
		$(CXX) $(ALL_CXXFLAGS) -c tests/gomoku_test.cpp -o tests/gomoku_test.o

test: 		$(TEST_TARGET) $(DAEMON_TEST_TARGET) $(TARGET)
		@echo "=== Running Game Tests ==="
		@./$(TEST_TARGET) | GREP_COLOR=32 grep --color=always -E 'GomokuTest\.([A-Za-z_]*)|tests|results|PASSED|FAILED'
		@echo ""
		@echo "=== Running Daemon Tests ==="
		@./$(DAEMON_TEST_TARGET) | GREP_COLOR=32 grep --color=always -E 'Daemon[A-Za-z]*Test\.([A-Za-z_]*)|tests|results|PASSED|FAILED'

tests: 		test

# Daemon tests
$(DAEMON_TEST_TARGET): googletest $(JSONC_LIB) tests/daemon_test.o $(DAEMON_CORE) src/net/cli.o src/net/json_api.o src/net/test_client_utils.o src/net/logger.o | $(BINDIR)
		$(CXX) $(ALL_CXXFLAGS) tests/daemon_test.o $(DAEMON_CORE) src/net/cli.o src/net/json_api.o src/net/test_client_utils.o src/net/logger.o $(GTEST_LIB) $(GTEST_MAIN_LIB) $(JSONC_LIB) -pthread -o $(DAEMON_TEST_TARGET)

tests/daemon_test.o: googletest tests/daemon_test.cpp src/net/cli.h src/net/json_api.h
		$(CXX) $(DAEMON_TEST_CXXFLAGS) -c tests/daemon_test.cpp -o tests/daemon_test.o

test-daemon: 	$(DAEMON_TEST_TARGET)
		./$(DAEMON_TEST_TARGET) | GREP_COLOR=32 grep --color=always -E 'Daemon[A-Za-z]*Test\.([A-Za-z_]*)|tests|results|PASSED|FAILED'

clean:  	cmake-clean
		rm -rf $(BINDIR)
		rm -f $(OBJECTS) tests/gomoku_test.o
		rm -f $(DAEMON_CORE) $(DAEMON_NET) src/net/test_client_utils.o tests/daemon_test.o
		rm -rf tests/googletest
		find . -name '*.a' -type f -delete || true

cmake-clean:
		rm -rf $(BUILD_DIR)
		find . -name CMakeCache.txt -delete || true
		find . -name CMakeFiles -type d -exec rm -rf {} \; || true
		find . -name cmake_install.cmake -type d -exec rm -rf {} \; || true
		true
