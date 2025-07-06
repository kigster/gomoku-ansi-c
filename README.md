# Gomoku Game - C Implementation

A C implementation of the Gomoku (Five-in-a-Row) game featuring an AI opponent using the MiniMax algorithm with Alpha-Beta pruning.

## Features

- **Interactive Console Interface**: Unicode-based board display with keyboard controls
- **AI Opponent**: Intelligent AI using MiniMax algorithm with Alpha-Beta pruning
- **Pattern Recognition**: Advanced threat detection and evaluation system
- **Cross-platform**: Works on Linux, macOS, and other Unix-like systems
- **Comprehensive Testing**: Full test suite using Google Test framework

## Game Rules

Gomoku is a strategy game where players take turns placing stones on a 19x19 board. The goal is to be the first to get five stones in a row (horizontally, vertically, or diagonally).

- **Human plays first** (Black stones ✕)
- **AI plays second** (White stones ○)
- **Win condition**: First to get 5 in a row wins

## Building and Running

### Prerequisites

- GCC compiler
- Make
- Git (for downloading Google Test)

### Build the Game

```bash
# Clone or download the project
cd gomoku-c

# Build the game
make

# Run the game with default difficulty (medium)
./gomoku

# Run with specific difficulty level
./gomoku 1    # Easy (fast AI)
./gomoku 2    # Medium (default)
./gomoku 3    # Hard (slow but strong AI)

# Show help
./gomoku --help
```

### Build and Run Tests

```bash
# Build and run the test suite
make test

# Clean build files
make clean
```

## Difficulty Levels

The game supports three difficulty levels that control the AI's thinking depth:

| Level | Depth | Performance | Strength |
|-------|-------|-------------|----------|
| **1 - Easy** | 2 | Very Fast | Beginner-friendly |
| **2 - Medium** | 4 | Moderate | Good balance |
| **3 - Hard** | 7 | Slow | Strong opponent |

- **Easy**: AI responds almost instantly, suitable for casual play
- **Medium**: AI takes 1-5 seconds per move, default setting
- **Hard**: AI takes 5-30 seconds per move, plays at advanced level

## Game Controls

- **Arrow Keys**: Move cursor around the board
- **Space/Enter**: Place a stone at cursor position
- **ESC/Q**: Quit the game

## Project Structure

```
gomoku-c/
├── src/
│   ├── gomoku.h          # Header file with function declarations
│   ├── gomoku.c          # Core game logic and AI evaluation
│   └── main.c            # Main game loop and user interface
├── tests/
│   ├── gomoku_test.cpp   # Comprehensive test suite
│   └── googletest/       # Google Test framework
├── Makefile              # Build configuration
└── README.md             # This file
```

## AI Implementation

The AI uses several advanced techniques:

### MiniMax Algorithm with Alpha-Beta Pruning
- **Search Depth**: 4 levels (configurable)
- **Evaluation**: Pattern-based position assessment
- **Pruning**: Alpha-beta pruning for performance optimization
- **Smart First Move**: Random placement near human's move for speed and variety

### Pattern Recognition
The AI recognizes various threat patterns:
- **Five in a row**: Winning position (100,000 points)
- **Straight four**: Immediate win threat (50,000 points)
- **Three in a row**: Strong threat (1,000 points)
- **Broken patterns**: Partial threats with gaps
- **Combinations**: Multiple threats create powerful positions

### Search Space Optimization
The AI uses intelligent move pruning:
- **Proximity-based search**: Only considers moves within 3 cells of existing stones
- **Early game optimization**: Focuses on center area when board is empty
- **First move randomization**: AI's first move is placed randomly 1-2 squares from human's move
- **Dramatic performance boost**: Reduces search space from 225 to ~20-50 moves per turn

### Performance Optimizations
Advanced optimizations for faster AI thinking:
- **Move ordering**: Prioritizes winning moves, blocking moves, and center positions for better alpha-beta pruning
- **Incremental evaluation**: Only evaluates board positions near the last move instead of the entire board
- **Early termination**: Immediately selects winning moves and stops search for excellent positions
- **Optimized stone counting**: Eliminates redundant calculations during move generation
- **Win detection caching**: Faster terminal condition checking with depth-based scoring
- **Transparent AI thinking**: Visual progress indicators and move count display show AI analysis in real-time

### Threat Analysis
- **Multi-directional**: Analyzes horizontal, vertical, and diagonal patterns
- **Combination scoring**: Bonus points for multiple simultaneous threats
- **Defensive evaluation**: Considers opponent's threats

## Testing

The project includes a comprehensive test suite with 16 test cases covering:

- **Board initialization and management**
- **Win detection in all directions**
- **Pattern recognition and threat analysis**
- **Evaluation function accuracy**
- **MiniMax algorithm functionality**
- **Edge cases and corner scenarios**

### Test Results

All tests pass successfully:
- ✅ Board initialization
- ✅ Win detection (horizontal, vertical, diagonal)
- ✅ Pattern recognition
- ✅ Evaluation functions
- ✅ MiniMax algorithm
- ✅ Edge cases

## Performance

Performance varies by difficulty level:

| Difficulty | Search Depth | Response Time | Positions Evaluated |
|------------|--------------|---------------|---------------------|
| Easy       | 2            | < 0.1 seconds | ~10-25              |
| Medium     | 4            | 0.1-0.5 seconds | ~50-200            |
| Hard       | 7            | 0.2-2 seconds | ~200-800           |

**Note**: AI's first move is instant (< 0.1 seconds) regardless of difficulty level.
**Optimization Impact**: 3-5x faster response times with improved move ordering and incremental evaluation.

- **Board size**: 19x19 (361 positions)
- **Evaluation rate**: ~1000 positions per second (varies by hardware)

## Technical Details

### Core Functions

- `evaluate_position()`: Main board evaluation function
- `calc_score_at()`: Threat analysis for individual positions
- `has_winner()`: Win condition detection
- `minimax()`: MiniMax algorithm with alpha-beta pruning
- `find_best_ai_move()`: AI move selection

### Algorithm Complexity

- **Time Complexity**: O(b^d) where b is branching factor and d is depth
- **Space Complexity**: O(d) for recursion stack
- **Optimization**: Alpha-beta pruning reduces effective branching factor

## Future Enhancements

Potential improvements:
- [ ] Opening book for better early game play
- [ ] Transposition table for move caching
- [ ] Variable search depth based on game phase
- [ ] GUI interface
- [ ] Network multiplayer support
- [ ] Different difficulty levels

## License

This project is open source and available under the MIT License.

## Contributing

Contributions are welcome! Please feel free to submit issues, feature requests, or pull requests.

## Acknowledgments

- Pattern recognition algorithms adapted from traditional Gomoku AI techniques
- Google Test framework for comprehensive testing
- Unicode characters for enhanced visual display 