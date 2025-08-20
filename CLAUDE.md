# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a **modern C++23 implementation** of Gomoku (Five in a Row) that was successfully refactored from a C99 codebase. The project leverages advanced C++23 features while maintaining full compatibility with both traditional Make and CMake build systems.

## Development Commands

### Setup (First Time)

```bash
# Run setup script to install dependencies and Google Test
./tests/setup
```

### Build System

#### Traditional Make Build

```bash
# shows help and available targets
make
make help

# Build the game (uses Makefile)
make build

# Build with parallel jobs for faster compilation
make build --j 4

# Clean build artifacts
make clean

# Rebuild from scratch
make rebuild
```

#### CMake Build (Alternative)

```bash
# Build using CMake (creates build directory and runs cmake ..)
make cmake-build

# Clean CMake build directory
make cmake-clean

# Rebuild from scratch using CMake
make cmake-rebuild
```

### Testing

```bash
# Run all unit tests (traditional Make)
make test

# Run tests using CMake
make cmake-test

# Build test executable only
make test_gomoku
```

### Version Management

```bash
# Check current version
make version

# Create git tag for current version
make tag

# Create GitHub release
make release
```

## Code Architecture

### Modern C++23 Design

The project follows a clean modular architecture with modern C++23 features and clear separation of concerns:

- **main.cpp**: Simple orchestrator that initializes components and starts the game loop
- **gomoku.cpp/.hpp**: Core evaluation functions with C++23 concepts, templates, and `std::expected`
- **board.cpp/.hpp**: Template-based `Board` class with RAII, `ValidBoardSize` concept, and modern C++ containers
- **game.cpp/.hpp**: Game logic, state management, move validation with modern C++ features
- **ai.cpp/.hpp**: AI module with minimax search, alpha-beta pruning, and move prioritization
- **ui.cpp/.hpp**: User interface with `TerminalInput`/`TerminalDisplay` classes and `std::format`
- **cli.cpp/.hpp**: Command-line parsing using `std::expected`, `std::span`, and `std::string_view`

### Key Modern C++23 Features

- **Concepts**: `ValidBoardSize` concept for compile-time board size validation
- **Templates**: Template-based `Board<Size>` class with compile-time size validation
- **std::expected**: Error handling in CLI parsing and validation without exceptions
- **std::span**: Memory-safe view over contiguous sequences for argument parsing
- **std::string_view**: Efficient string handling without unnecessary copies
- **std::format**: Type-safe string formatting replacing C-style printf
- **enum class**: Type-safe enumerations (`Player`, `GameState`, `ThreatType`, `Difficulty`)
- **RAII**: Automatic memory management in `Board` class eliminating manual malloc/free
- **constexpr**: Compile-time computation for constants and simple functions

### AI Implementation

The AI uses minimax with alpha-beta pruning and several optimizations:

- **Pattern Recognition**: Threat-based evaluation using matrices (THREAT_FIVE, THREAT_STRAIGHT_FOUR, etc.)
- **Search Optimization**: Only evaluates moves within SEARCH_RADIUS (4 cells) of existing stones
- **Move Ordering**: Prioritizes winning moves and threats for better pruning efficiency
- **Timeout Support**: Configurable time limits with graceful degradation
- **Incremental Evaluation**: Only evaluates positions near the last move for performance

### Build Configuration

- **C++ Standard**: C++23 (`-std=c++23`) with full modern feature support
- **Compiler**: GCC/Clang with optimization flags (-O3 for game, -O2 for tests)
- **Warning Flags**: -Wall -Wextra -Wpedantic -Wunused-parameter for strict code quality
- **Libraries**: Math library (-lm) and pthread for threading support
- **Test Framework**: Google Test (C++) for comprehensive testing
- **Dual Build Systems**: Both traditional Makefile and CMake with consistent C++23 configuration

### Testing Structure

- **20 comprehensive tests** covering all game components
- **Test Categories**: Board management, move validation, win detection, pattern recognition, AI algorithm, undo functionality
- **Integration Tests**: Full game scenarios and edge cases
- **Performance Tests**: AI timing and evaluation metrics

## Key Constants and Configuration

### Game Settings

- **Board Sizes**: 15x15 or 19x19 (configurable via --board flag)
- **Difficulty Levels**: Easy (depth 2), Medium (depth 4), Hard (depth 6)
- **Search Parameters**: SEARCH_RADIUS=4, NEED_TO_WIN=5, NUM_DIRECTIONS=4
- **Unicode Display**: Full Unicode support for board rendering

### AI Configuration

- **Score Constants**: WIN_SCORE=1000000, LOSE_SCORE=-1000000
- **Threat Types**: Graduated threat levels from THREAT_FIVE to THREAT_TWO
- **Player Constants**: AI_CELL_CROSSES=1, AI_CELL_NAUGHTS=-1, AI_CELL_EMPTY=0

## Development Notes

### Memory Management

- Dynamic board allocation using create_board() and free_board()
- Proper cleanup in all exit paths
- History arrays with MAX_MOVE_HISTORY=400 limit

### Performance Considerations

- AI search limited to proximity of existing stones for efficiency
- Incremental evaluation for faster position assessment
- Alpha-beta pruning with intelligent move ordering
- Timeout mechanisms to prevent long search times

### Modern C++23 Refactoring

This project underwent a complete transformation from C99 to modern C++23:

#### Migration Strategy
- **Incremental Conversion**: Files converted in pairs (header + implementation) 
- **Mixed Language Support**: Temporary `extern "C"` linkage during transition
- **Full C++23 Adoption**: Complete elimination of C-style code and headers
- **Build System Updates**: Both Makefile and CMake reconfigured for C++23

#### Key Architectural Improvements
- **Memory Safety**: RAII replacing manual memory management
- **Type Safety**: Strong typing with concepts and enum classes  
- **Error Handling**: `std::expected` replacing error codes
- **Performance**: Template specialization and compile-time validation
- **Maintainability**: Modern C++ idioms and clear interfaces

#### Code Style
- **Function naming**: snake_case with descriptive names (maintained)
- **Constants**: `inline constexpr` variables replacing macros
- **Error handling**: `std::expected<T, Error>` for recoverable errors
- **Header guards**: `#pragma once` for simplicity
- **Namespaces**: `gomoku::` namespace for organization
