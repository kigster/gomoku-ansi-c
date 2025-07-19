# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Development Commands

### Setup (First Time)

```bash
# Run setup script to install dependencies and Google Test
./tests/setup
```

### Build System

#### Traditional Make Build

```bash
# Build the game (uses Makefile)
make

# Build with parallel jobs for faster compilation
make -j 4

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

### Modular Design

The project follows a clean modular architecture with clear separation of concerns:

- **main.c** (105 lines): Simple orchestrator that initializes components and starts the game loop
- **gomoku.c/.h**: Core evaluation functions for minimax algorithm and pattern recognition
- **board.c/.h**: Board management, memory allocation, and coordinate utilities
- **game.c/.h**: Game logic, state management, move validation, and timing
- **ai.c/.h**: AI module with minimax search, alpha-beta pruning, and move prioritization
- **ui.c/.h**: User interface, display rendering, and keyboard input handling
- **cli.c/.h**: Command-line argument parsing and configuration

### Key Data Structures

- **game_state_t**: Central game state containing board, config, history, and timing
- **move_history_t**: Move tracking with timing and evaluation metrics
- **cli_config_t**: Configuration from command-line arguments
- **interesting_move_t**: AI search optimization for move prioritization

### AI Implementation

The AI uses minimax with alpha-beta pruning and several optimizations:

- **Pattern Recognition**: Threat-based evaluation using matrices (THREAT_FIVE, THREAT_STRAIGHT_FOUR, etc.)
- **Search Optimization**: Only evaluates moves within SEARCH_RADIUS (4 cells) of existing stones
- **Move Ordering**: Prioritizes winning moves and threats for better pruning efficiency
- **Timeout Support**: Configurable time limits with graceful degradation
- **Incremental Evaluation**: Only evaluates positions near the last move for performance

### Build Configuration

- **Compiler**: GCC with optimization flags (-O3 for game, -O2 for tests)
- **Warning Flags**: -Wall -Wextra -Wunused-parameter -Wimplicit-function-declaration
- **Libraries**: Math library (-lm) for evaluation functions
- **Test Framework**: Google Test (C++) for comprehensive testing

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

### Code Style

- Function naming: snake_case with descriptive names
- Constants: ALL*CAPS with prefixes (GAME*, AI*, THREAT*)
- Error handling: RT_SUCCESS/RT_FAILURE return codes
- Header guards: Standard #ifndef/#define pattern
