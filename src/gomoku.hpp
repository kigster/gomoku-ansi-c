//
//  gomoku.hpp
//  gomoku - Modern C++23 Core Types and Constants
//
//  Refactored from original C99 implementation with modern C++ features
//

#pragma once

#include <array>
#include <concepts>
#include <expected>
#include <format>
#include <ranges>
#include <string_view>
#include <chrono>
#include <cstdint>
#include <algorithm>
#include <memory>
#include <span>

namespace gomoku {

//===============================================================================
// GAME METADATA
//===============================================================================

inline constexpr std::string_view GAME_NAME = "Gomoku";
inline constexpr std::string_view GAME_VERSION = "2.0.0";
inline constexpr std::string_view GAME_AUTHOR = "Konstantin Gredeskoul";
inline constexpr std::string_view GAME_LICENSE = "MIT License";
inline constexpr std::string_view GAME_URL = "https://github.com/kigster/gomoku-ansi-c";
inline constexpr std::string_view GAME_DESCRIPTION = "Gomoku, also known as Five in a Row";

inline constexpr std::string_view GAME_COPYRIGHT = "© 2025 Konstantin Gredeskoul, MIT License";

inline constexpr std::string_view GAME_RULES_BRIEF = R"(
  ↑ ↓ ← → (arrows) ───→ to move around, 
  Enter or Space   ───→ to make a move, 
  U                ───→ to undo last move pair (if --undo is enabled), 
  ?                ───→ to show game rules, 
  ESC              ───→ to quit game.
)";

inline constexpr std::string_view GAME_RULES_LONG = R"(
Gomoku, also known as Five in a Row, is a two-player strategy board game. 
The objective is to get five crosses or naughts in a row, either horizontally,
vertically, or diagonally. The game is played on a 15x15 grid, or 19x19 
grid, with each player taking turns placing their crosses or naughts. The 
first player to get five crosses or naughts in a row wins the game.

In this version you get to always play X which gives you a slight advantage.
The computer will play O (and will go second). Slightly brigher O denotes the
computer's last move (you can Undo moves if you enable Undo).
)";

//===============================================================================
// GAME CONSTANTS
//===============================================================================

inline constexpr int DEFAULT_BOARD_SIZE = 19;
inline constexpr int SEARCH_RADIUS = 4;
inline constexpr int NEED_TO_WIN = 5;
inline constexpr int NUM_DIRECTIONS = 4;

// UI Constants
inline constexpr std::string_view UNICODE_CROSSES = "✕";
inline constexpr std::string_view UNICODE_NAUGHTS = "○";
inline constexpr std::string_view UNICODE_CURSOR = "✕";
inline constexpr std::string_view UNICODE_OCCUPIED = "\033[0;33m◼︎";

// Note: GAME_VERSION, GAME_COPYRIGHT, GAME_URL are defined earlier in the file

//===============================================================================
// TYPE DEFINITIONS AND CONCEPTS
//===============================================================================

// Player type enum class for type safety
enum class Player : int8_t {
    Empty = 0,
    Cross = 1,      // Human player (X) [[memory:2632966]]
    Naught = -1     // AI player (O)
};

// Board size concept
template<int Size>
concept ValidBoardSize = (Size == 15 || Size == 19);

// Coordinate concept
template<typename T>
concept Coordinate = std::integral<T> && std::signed_integral<T>;

// Position structure with C++23 features
struct Position {
    int x, y;
    
    constexpr Position() noexcept = default;
    constexpr Position(int x_, int y_) noexcept : x(x_), y(y_) {}
    
    constexpr auto operator<=>(const Position&) const = default;
    
    constexpr bool is_valid(int board_size) const noexcept {
        return x >= 0 && y >= 0 && x < board_size && y < board_size;
    }
    
    constexpr Position operator+(const Position& other) const noexcept {
        return {x + other.x, y + other.y};
    }
    
    constexpr Position operator*(int factor) const noexcept {
        return {x * factor, y * factor};
    }
};

// Direction vectors for line checking
inline constexpr std::array<Position, NUM_DIRECTIONS> DIRECTIONS = {{
    {1, 0},   // horizontal
    {0, 1},   // vertical
    {1, 1},   // diagonal
    {1, -1}   // diagonal
}};

//===============================================================================
// GAME STATE ENUMS
//===============================================================================

enum class GameState : uint8_t {
    Running,
    HumanWin,
    AIWin,
    Draw,
    Quit
};

enum class ThreatType : uint8_t {
    Nothing = 0,
    Five = 1,
    StraightFour = 2,
    Four = 3,
    Three = 4,
    FourBroken = 5,
    ThreeBroken = 6,
    Two = 7,
    NearEnemy = 8,
    ThreeAndFour = 9,
    ThreeAndThree = 10,
    ThreeAndThreeBroken = 11
};

enum class Difficulty : uint8_t {
    Easy = 2,
    Medium = 4,
    Hard = 6
};

//===============================================================================
// SCORE CONSTANTS
//===============================================================================

inline constexpr int WIN_SCORE = 1'000'000;
inline constexpr int LOSE_SCORE = -1'000'000;
inline constexpr int MAX_DEPTH = 10;
inline constexpr int DEPTH_WARNING_THRESHOLD = 7;

//===============================================================================
// UNICODE DISPLAY CONSTANTS
//===============================================================================

namespace unicode {
    inline constexpr std::string_view EMPTY = "·";
    inline constexpr std::string_view CROSSES = "✕";  // [[memory:2632966]]
    inline constexpr std::string_view NAUGHTS = "○";
    inline constexpr std::string_view CURSOR = "✕";
    inline constexpr std::string_view OCCUPIED = "\033[0;33m◼︎";
    
    // Board border characters
    inline constexpr std::string_view CORNER_TL = "┌";
    inline constexpr std::string_view CORNER_TR = "┐";
    inline constexpr std::string_view CORNER_BL = "└";
    inline constexpr std::string_view CORNER_BR = "┘";
    inline constexpr std::string_view EDGE_H = "─";
    inline constexpr std::string_view EDGE_V = "│";
    inline constexpr std::string_view T_TOP = "┬";
    inline constexpr std::string_view T_BOT = "┴";
    inline constexpr std::string_view T_LEFT = "├";
    inline constexpr std::string_view T_RIGHT = "┤";
}

//===============================================================================
// KEY CODES
//===============================================================================

namespace keys {
    inline constexpr int ESC = 27;
    inline constexpr int ENTER = 13;
    inline constexpr int SPACE = 32;
    inline constexpr int UP = 72;
    inline constexpr int DOWN = 80;
    inline constexpr int LEFT = 75;
    inline constexpr int RIGHT = 77;
    inline constexpr int CTRL_Z = 26;
}

//===============================================================================
// UTILITY FUNCTIONS
//===============================================================================

constexpr Player other_player(Player player) noexcept {
    return (player == Player::Cross) ? Player::Naught : Player::Cross;
}

constexpr std::string_view player_to_unicode(Player player) noexcept {
    switch (player) {
        case Player::Cross: return unicode::CROSSES;
        case Player::Naught: return unicode::NAUGHTS;
        default: return unicode::EMPTY;
    }
}

//===============================================================================
// RESULT TYPES
//===============================================================================

template<typename T>
using Result = std::expected<T, std::string>;

// Common result types
using VoidResult = Result<void>;
using MoveResult = Result<Position>;

//===============================================================================
// TIMING UTILITIES
//===============================================================================

using TimePoint = std::chrono::steady_clock::time_point;
using Duration = std::chrono::duration<double>;

inline TimePoint now() noexcept {
    return std::chrono::steady_clock::now();
}

inline double elapsed_seconds(TimePoint start, TimePoint end = now()) noexcept {
    return std::chrono::duration_cast<Duration>(end - start).count();
}

//===============================================================================
// EVALUATION AND PATTERN ANALYSIS
//===============================================================================

// Forward declarations for board evaluation
template<int Size> requires ValidBoardSize<Size>
class Board;

/**
 * Main evaluation function for minimax algorithm.
 * Evaluates the entire board position from the perspective of the given player.
 */
template<int Size> requires ValidBoardSize<Size>
[[nodiscard]] int evaluate_position(const Board<Size>& board, Player player);

/**
 * Fast incremental evaluation function for minimax algorithm.
 * Only evaluates positions near the last move for better performance.
 */
template<int Size> requires ValidBoardSize<Size>
[[nodiscard]] int evaluate_position_incremental(const Board<Size>& board, Player player, 
                                               const Position& last_move);

/**
 * Calculates the threat score for a stone at position.
 * Analyzes patterns in all four directions from the given position.
 */
template<int Size> requires ValidBoardSize<Size>
[[nodiscard]] int calc_score_at(const Board<Size>& board, Player player, const Position& pos);

/**
 * Analyzes a single line/direction for threat patterns.
 */
[[nodiscard]] ThreatType calc_threat_in_one_dimension(std::span<const Player> line, Player player);

/**
 * Calculates additional score for combinations of threats.
 */
[[nodiscard]] int calc_combination_threat(ThreatType one, ThreatType two);

/**
 * Initializes the threat scoring matrix.
 * Must be called before using evaluation functions.
 */
void populate_threat_matrix();

/**
 * Display game rules in formatted output.
 */
void display_rules();

//===============================================================================
// MINIMAX ALGORITHM INTERFACE
//===============================================================================

/**
 * Minimax with alpha-beta pruning for C++ interface
 */
template<int Size> requires ValidBoardSize<Size>
[[nodiscard]] int minimax_with_last_move(const Board<Size>& board, int depth, 
                                         int alpha, int beta, bool maximizing_player,
                                         Player ai_player, const Position& last_move);

//===============================================================================
// LEGACY C INTERFACE (for gradual migration)
//===============================================================================

// Utility functions
std::string_view get_coordinate_unicode(int index) noexcept;

} // namespace gomoku

//===============================================================================
// C-COMPATIBLE INTERFACE (declared in global namespace)
//===============================================================================

// Note: cli_config_t and game_state_t are defined in cli.hpp and game.h respectively

// These functions provide C-compatible interface during migration
extern "C" {
    int evaluate_position(int** board, int size, int player);
    int evaluate_position_incremental(int** board, int size, int player, int last_x, int last_y);
    int has_winner(int** board, int size, int player);
    int calc_score_at(int** board, int size, int player, int x, int y);
    int calc_threat_in_one_dimension(int* row, int player);
    int calc_combination_threat(int one, int two);
    int other_player(int player);
    void reset_row(int* row, int size);
    int minimax_with_last_move(int** board, int depth, int alpha, int beta,
                              int maximizing_player, int ai_player, int last_x, int last_y);
    int minimax_example(int** board, int size, int depth, int alpha, int beta,
                       int maximizing_player, int ai_player);
    // Board management functions
    int** create_board(int size);
    void free_board(int** board, int size);
    int is_valid_move(int** board, int x, int y, int size);
    int board_to_display_coord(int board_coord);
    int display_to_board_coord(int display_coord);
    
    // Note: Game state and AI functions are declared in game.h and ai.h respectively
}

//===============================================================================
// HASH SUPPORT
//===============================================================================

// Hash specializations for standard containers
template<>
struct std::hash<gomoku::Position> {
    std::size_t operator()(const gomoku::Position& pos) const noexcept {
        return std::hash<int>{}(pos.x) ^ (std::hash<int>{}(pos.y) << 1);
    }
};

template<>
struct std::formatter<gomoku::Position> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }
    
    auto format(const gomoku::Position& pos, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "({}, {})", pos.x, pos.y);
    }
};