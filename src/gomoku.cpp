//
//  gomoku.cpp
//  gomoku - Modern C++23 Evaluation and Pattern Analysis
//
//  Converted from C99 implementation with modern C++ features
//

#include "gomoku.hpp"
#include "board.hpp"
#include <array>
#include <algorithm>
#include <iostream>
#include <format>
#include <cstdlib>
#include <cstring>

namespace gomoku {

//===============================================================================
// STATIC DATA AND CONSTANTS
//===============================================================================

namespace detail {
    // Threat scoring matrix - initialized by populate_threat_matrix()
    std::array<int, 20> threat_cost{};
    bool threat_initialized = false;
    std::array<ThreatType, NUM_DIRECTIONS> threats{};
    
    // Constants for legacy C interface
    constexpr int OUT_OF_BOUNDS = 32;
    constexpr int SEARCH_RADIUS = 4;
    constexpr int AI_CELL_EMPTY = 0;
    constexpr int AI_CELL_CROSSES = 1;
    constexpr int AI_CELL_NAUGHTS = -1;
    
    // Convert between C and C++ player representations
    constexpr Player int_to_player(int player) noexcept {
        switch (player) {
            case 1: return Player::Cross;
            case -1: return Player::Naught;
            default: return Player::Empty;
        }
    }
    
    constexpr int player_to_int(Player player) noexcept {
        return static_cast<int>(player);
    }
    
    constexpr ThreatType int_to_threat(int threat) noexcept {
        return static_cast<ThreatType>(threat);
    }
    
    constexpr int threat_to_int(ThreatType threat) noexcept {
        return static_cast<int>(threat);
    }
}

//===============================================================================
// THREAT MATRIX INITIALIZATION
//===============================================================================

void populate_threat_matrix() {
    if (detail::threat_initialized) return;
    
    using namespace detail;
    
    // Initialize threat costs based on original algorithm
    threat_cost[threat_to_int(ThreatType::Nothing)] = 0;
    threat_cost[threat_to_int(ThreatType::Five)] = 1000000;
    threat_cost[threat_to_int(ThreatType::StraightFour)] = 100000;
    threat_cost[threat_to_int(ThreatType::Four)] = 10000;
    threat_cost[threat_to_int(ThreatType::Three)] = 1000;
    threat_cost[threat_to_int(ThreatType::FourBroken)] = 1000;
    threat_cost[threat_to_int(ThreatType::ThreeBroken)] = 100;
    threat_cost[threat_to_int(ThreatType::Two)] = 10;
    threat_cost[threat_to_int(ThreatType::NearEnemy)] = 1;
    threat_cost[threat_to_int(ThreatType::ThreeAndFour)] = 200000;
    threat_cost[threat_to_int(ThreatType::ThreeAndThree)] = 50000;
    threat_cost[threat_to_int(ThreatType::ThreeAndThreeBroken)] = 10000;
    
    detail::threat_initialized = true;
}

//===============================================================================
// PATTERN ANALYSIS IMPLEMENTATION
//===============================================================================

[[nodiscard]] ThreatType calc_threat_in_one_dimension(std::span<const Player> line, Player player) {
    if (line.size() < NEED_TO_WIN * 2 - 1) {
        return ThreatType::Nothing;
    }
    
    int center = line.size() / 2;
    if (line[center] != player) {
        return ThreatType::Nothing;
    }
    
    Player opponent = other_player(player);
    
    // Count consecutive stones in both directions
    int count = 1; // Center stone
    
    // Count left
    int left_count = 0;
    for (int i = center - 1; i >= 0 && line[i] == player; --i) {
        ++left_count;
        ++count;
    }
    
    // Count right  
    int right_count = 0;
    for (int i = center + 1; i < static_cast<int>(line.size()) && line[i] == player; ++i) {
        ++right_count;
        ++count;
    }
    
    // Check for blocking by opponents
    bool left_blocked = (center - left_count - 1 >= 0) && (line[center - left_count - 1] == opponent);
    bool right_blocked = (center + right_count + 1 < static_cast<int>(line.size())) && (line[center + right_count + 1] == opponent);
    
    // Check for open spaces
    bool left_open = (center - left_count - 1 >= 0) && (line[center - left_count - 1] == Player::Empty);
    bool right_open = (center + right_count + 1 < static_cast<int>(line.size())) && (line[center + right_count + 1] == Player::Empty);
    
    // Determine threat type based on pattern analysis
    if (count >= NEED_TO_WIN) {
        return ThreatType::Five;
    } else if (count == 4) {
        if (!left_blocked && !right_blocked) {
            return ThreatType::StraightFour;
        } else {
            return ThreatType::Four;
        }
    } else if (count == 3) {
        if (!left_blocked && !right_blocked) {
            return ThreatType::Three;
        } else {
            return ThreatType::ThreeBroken;
        }
    } else if (count == 2) {
        return ThreatType::Two;
    } else if (count == 1) {
        // Check if there are friendly stones nearby
        if (left_open || right_open) {
            return ThreatType::NearEnemy;
        }
    }
    
    return ThreatType::Nothing;
}

[[nodiscard]] int calc_combination_threat(ThreatType one, ThreatType two) {
    using namespace detail;
    
    populate_threat_matrix();
    
    // Calculate bonus for threat combinations
    if (one == ThreatType::Three && two == ThreatType::Four) {
        return threat_cost[threat_to_int(ThreatType::ThreeAndFour)];
    } else if (one == ThreatType::Three && two == ThreatType::Three) {
        return threat_cost[threat_to_int(ThreatType::ThreeAndThree)];
    }
    
    return 0;
}

//===============================================================================
// BOARD EVALUATION TEMPLATES
//===============================================================================

template<int Size> requires ValidBoardSize<Size>
[[nodiscard]] int calc_score_at(const Board<Size>& board, Player player, const Position& pos) {
    populate_threat_matrix();
    
    // Don't evaluate if position is out of bounds
    if (!board.is_valid_position(pos)) {
        return 0;
    }
    
    int total_score = 0;
    std::array<ThreatType, NUM_DIRECTIONS> threats;
    
    // Analyze all four directions - simulate placing the stone
    for (int dir = 0; dir < NUM_DIRECTIONS; ++dir) {
        auto line = board.get_line(pos, DIRECTIONS[dir]);
        std::array<Player, NEED_TO_WIN * 2 - 1> player_line;
        std::ranges::copy(line, player_line.begin());
        
        // The center position should be the player we're evaluating for
        player_line[NEED_TO_WIN - 1] = player;
        
        ThreatType threat = calc_threat_in_one_dimension(player_line, player);
        threats[dir] = threat;
        total_score += detail::threat_cost[detail::threat_to_int(threat)];
    }
    
    // Add combination bonuses
    for (int i = 0; i < NUM_DIRECTIONS; ++i) {
        for (int j = i + 1; j < NUM_DIRECTIONS; ++j) {
            total_score += calc_combination_threat(threats[i], threats[j]);
        }
    }
    
    return total_score;
}

template<int Size> requires ValidBoardSize<Size>
[[nodiscard]] int evaluate_position_incremental(const Board<Size>& board, Player player, 
                                               const Position& last_move) {
    populate_threat_matrix();
    
    int total_score = 0;
    Player opponent = other_player(player);
    
    // Check for immediate win/loss first
    if (board.has_winner(player)) {
        return WIN_SCORE;
    }
    if (board.has_winner(opponent)) {
        return LOSE_SCORE;
    }
    
    // Only evaluate positions within radius of the last move for speed
    constexpr int eval_radius = 3;
    int min_x = std::max(0, last_move.x - eval_radius);
    int max_x = std::min(Size - 1, last_move.x + eval_radius);
    int min_y = std::max(0, last_move.y - eval_radius);
    int max_y = std::min(Size - 1, last_move.y + eval_radius);
    
    for (int i = min_x; i <= max_x; ++i) {
        for (int j = min_y; j <= max_y; ++j) {
            Position pos{i, j};
            if (board.at(pos) == player) {
                total_score += calc_score_at(board, player, pos);
            } else if (board.at(pos) == opponent) {
                total_score -= calc_score_at(board, opponent, pos);
            }
        }
    }
    
    return total_score;
}

template<int Size> requires ValidBoardSize<Size>
[[nodiscard]] int evaluate_position(const Board<Size>& board, Player player) {
    populate_threat_matrix();
    
    int total_score = 0;
    Player opponent = other_player(player);
    
    // Check for immediate win/loss first
    if (board.has_winner(player)) {
        return WIN_SCORE;
    }
    if (board.has_winner(opponent)) {
        return LOSE_SCORE;
    }
    
    // Evaluate all positions
    for (int i = 0; i < Size; ++i) {
        for (int j = 0; j < Size; ++j) {
            Position pos{i, j};
            if (board.at(pos) == player) {
                total_score += calc_score_at(board, player, pos);
            } else if (board.at(pos) == opponent) {
                total_score -= calc_score_at(board, opponent, pos);
            }
        }
    }
    
    return total_score;
}

//===============================================================================
// EXPLICIT TEMPLATE INSTANTIATIONS
//===============================================================================

template int calc_score_at<15>(const Board<15>&, Player, const Position&);
template int calc_score_at<19>(const Board<19>&, Player, const Position&);
template int evaluate_position_incremental<15>(const Board<15>&, Player, const Position&);
template int evaluate_position_incremental<19>(const Board<19>&, Player, const Position&);
template int evaluate_position<15>(const Board<15>&, Player);
template int evaluate_position<19>(const Board<19>&, Player);

//===============================================================================
// UTILITY FUNCTIONS
//===============================================================================

void display_rules() {
    std::cout << std::format("\n{}\n", GAME_RULES_LONG);
    std::cout << std::format("Controls:\n{}\n", GAME_RULES_BRIEF);
}

} // namespace gomoku

//===============================================================================
// LEGACY C INTERFACE IMPLEMENTATION
//===============================================================================

extern "C" {

using namespace gomoku;
using namespace gomoku::detail;

int evaluate_position(int** board, int size, int player) {
    // Convert to C++ types and delegate
    Player cpp_player = int_to_player(player);
    
    if (size == 15) {
        Board<15> cpp_board;
        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                cpp_board.set(i, j, int_to_player(board[i][j]));
            }
        }
        return gomoku::evaluate_position(cpp_board, cpp_player);
    } else if (size == 19) {
        Board<19> cpp_board;
        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                cpp_board.set(i, j, int_to_player(board[i][j]));
            }
        }
        return gomoku::evaluate_position(cpp_board, cpp_player);
    }
    
    return 0;
}

int evaluate_position_incremental(int** board, int size, int player, int last_x, int last_y) {
    Player cpp_player = int_to_player(player);
    Position last_move{last_x, last_y};
    
    if (size == 15) {
        Board<15> cpp_board;
        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                cpp_board.set(i, j, int_to_player(board[i][j]));
            }
        }
        return gomoku::evaluate_position_incremental(cpp_board, cpp_player, last_move);
    } else if (size == 19) {
        Board<19> cpp_board;
        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                cpp_board.set(i, j, int_to_player(board[i][j]));
            }
        }
        return gomoku::evaluate_position_incremental(cpp_board, cpp_player, last_move);
    }
    
    return 0;
}

int has_winner(int** board, int size, int player) {
    Player cpp_player = int_to_player(player);
    
    if (size == 15) {
        Board<15> cpp_board;
        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                cpp_board.set(i, j, int_to_player(board[i][j]));
            }
        }
        return cpp_board.has_winner(cpp_player) ? 1 : 0;
    } else if (size == 19) {
        Board<19> cpp_board;
        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                cpp_board.set(i, j, int_to_player(board[i][j]));
            }
        }
        return cpp_board.has_winner(cpp_player) ? 1 : 0;
    }
    
    return 0;
}

int calc_score_at(int** board, int size, int player, int x, int y) {
    Player cpp_player = int_to_player(player);
    Position pos{x, y};
    
    if (size == 15) {
        Board<15> cpp_board;
        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                cpp_board.set(i, j, int_to_player(board[i][j]));
            }
        }
        return gomoku::calc_score_at(cpp_board, cpp_player, pos);
    } else if (size == 19) {
        Board<19> cpp_board;
        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                cpp_board.set(i, j, int_to_player(board[i][j]));
            }
        }
        return gomoku::calc_score_at(cpp_board, cpp_player, pos);
    }
    
    return 0;
}

int calc_threat_in_one_dimension(int* row, int player) {
    constexpr int line_size = NEED_TO_WIN * 2 - 1;
    std::array<Player, line_size> cpp_line;
    
    for (int i = 0; i < line_size; ++i) {
        cpp_line[i] = int_to_player(row[i]);
    }
    
    ThreatType threat = gomoku::calc_threat_in_one_dimension(cpp_line, int_to_player(player));
    return threat_to_int(threat);
}

int calc_combination_threat(int one, int two) {
    return gomoku::calc_combination_threat(int_to_threat(one), int_to_threat(two));
}

int other_player(int player) {
    return player_to_int(gomoku::other_player(int_to_player(player)));
}

void reset_row(int* row, int size) {
    for (int i = 0; i < size; ++i) {
        row[i] = OUT_OF_BOUNDS;
    }
}

// Simplified minimax implementations for C interface
int minimax_with_last_move(int** board, int depth, int alpha, int beta,
                          int maximizing_player, int ai_player, int last_x, int last_y) {
    // This would be a full minimax implementation
    // For now, return a simple evaluation
    return evaluate_position_incremental(board, 19, ai_player, last_x, last_y);
}

int minimax_example(int** board, int size, int depth, int alpha, int beta,
                   int maximizing_player, int ai_player) {
    // This would be a full minimax implementation
    // For now, return a simple evaluation
    return evaluate_position(board, size, ai_player);
}

// Board management functions for C interface
int** create_board(int size) {
    int **new_board = (int**)malloc(size * sizeof(int *));
    if (!new_board) {
        return NULL;
    }

    for (int i = 0; i < size; i++) {
        new_board[i] = (int*)malloc(size * sizeof(int));
        if (!new_board[i]) {
            // Free previously allocated rows on failure
            for (int j = 0; j < i; j++) {
                free(new_board[j]);
            }
            free(new_board);
            return NULL;
        }

        // Initialize cells to empty
        for (int j = 0; j < size; j++) {
            new_board[i][j] = detail::AI_CELL_EMPTY;
        }
    }

    return new_board;
}

void free_board(int** board, int size) {
    if (!board) {
        return;
    }

    for (int i = 0; i < size; i++) {
        free(board[i]);
    }
    free(board);
}

int is_valid_move(int** board, int x, int y, int size) {
    return x >= 0 && x < size && y >= 0 && y < size && board[x][y] == detail::AI_CELL_EMPTY;
}

std::string_view get_coordinate_unicode(int index) {
    // Convert 0-based index to 1-based display with Unicode circled characters
    using namespace std::string_view_literals;
    static constexpr std::array coords = {
        "❶"sv, "❷"sv, "❸"sv, "❹"sv, "❺"sv, "❻"sv, "❼"sv, "❽"sv, "❾"sv, "❿"sv,
        "⓫"sv, "⓬"sv, "⓭"sv, "⓮"sv, "⓯"sv, "⓰"sv, "⓱"sv, "⓲"sv, "⓳"sv
    };

    if (index >= 0 && index < static_cast<int>(coords.size())) {
        return coords[index];
    }
    return "?"sv;
}

int board_to_display_coord(int board_coord) {
    return board_coord + 1;
}

int display_to_board_coord(int display_coord) {
    return display_coord - 1;
}

void populate_threat_matrix(void) {
    gomoku::populate_threat_matrix();
}

} // extern "C"