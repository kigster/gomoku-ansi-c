//
//  minimax_evaluation.h
//  gomoku - Header file for minimax evaluation functions
//
//  Refactored from original heuristics.h for minimax algorithm
//

#ifndef GOMOKU_H
#define GOMOKU_H

#include <math.h>
#include "ansi.h"

//===============================================================================
// GAME CONSTANTS
//===============================================================================

#define GAME_NAME "Gomoku"
#define GAME_BINARY "gomoku"
#define GAME_VERSION "1.0.0"
#define GAME_AUTHOR "Konstantin Gredeskoul"
#define GAME_LICENSE "MIT License"
#define GAME_URL "https://github.com/kigster/gomoku-ansi-c"
#define GAME_DESCRIPTION "Gomoku, also known as Five in a Row"
#define GAME_COPYRIGHT "© 2025-2026 Konstantin Gredeskoul, MIT License"
#define GAME_RULES_BRIEF \
    " ↑ ↓ ← → (arrows) ───→ to move around, \n" \
    "  Enter or Space   ───→ to make a move, \n" \
    "  U                ───→ to undo last move pair (if --undo is enabled), \n" \
    "  ?                ───→ to show game rules, \n" \
    "  ESC              ───→ to quit game." 
#define GAME_RULES_LONG                                                          \
    "Gomoku, also known as Five in a Row, is a two-player strategy board game. \n " \
    "The objective is to get five crosses or naughts in a row, either horizontally,\n " \
    "vertically, or diagonally. The game is played on a 15x15 grid, or 19x19 \n "   \
    "grid, with each player taking turns placing their crosses or naughts. The \n " \
    "first player to get five crosses or naughts in a row wins the game.\n\n " \
    "In this version you get to always play X which gives you a slight advantage.\n " \
    "The computer will play O (and will go second). Slightly brigher O denotes the\n " \
    "computer's last move (you can Undo moves if you enable Undo).\n"

// Board size is now configurable via command line, default is 19
#define DEFAULT_BOARD_SIZE 19

//===============================================================================
// CONSTANTS AND DEFINITIONS
//===============================================================================

// Board cell values
#define AI_CELL_EMPTY 0
#define AI_CELL_CROSSES 1
#define AI_CELL_NAUGHTS -1

// Player types
typedef enum {
    PLAYER_TYPE_HUMAN = 0,
    PLAYER_TYPE_AI = 1
} player_type_t;

// Search parameters
#define SEARCH_RADIUS 4
#define NEED_TO_WIN 5
#define NUM_DIRECTIONS 4

// Return codes
#define RT_SUCCESS 0
#define RT_FAILURE -1
#define RT_BREAK 1
#define RT_CONTINUE 0

// Internal constants
#define OUT_OF_BOUNDS 32

// Threat type definitions
#define THREAT_NOTHING 0
#define THREAT_FIVE 1
#define THREAT_STRAIGHT_FOUR 2
#define THREAT_FOUR 3
#define THREAT_THREE 4
#define THREAT_FOUR_BROKEN 5
#define THREAT_THREE_BROKEN 6
#define THREAT_TWO 7
#define THREAT_NEAR_ENEMY 8
#define THREAT_THREE_AND_FOUR 9
#define THREAT_THREE_AND_THREE 10
#define THREAT_THREE_AND_THREE_BROKEN 11

// Score constants for minimax
#define WIN_SCORE 1000000
#define LOSE_SCORE -1000000

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

//===============================================================================
// GAME CONSTANTS
//===============================================================================

// Unicode characters for board display
#define UNICODE_EMPTY "·"
#define UNICODE_CROSSES "✕"
#define UNICODE_NAUGHTS "○"
#define UNICODE_CORNER_TL "┌"
#define UNICODE_CORNER_TR "┐"
#define UNICODE_CORNER_BL "└"
#define UNICODE_CORNER_BR "┘"
#define UNICODE_EDGE_H "─"
#define UNICODE_EDGE_V "│"
#define UNICODE_T_TOP "┬"
#define UNICODE_T_BOT "┴"
#define UNICODE_T_LEFT "├"
#define UNICODE_T_RIGHT "┤"

// Key codes
#define KEY_ESC 27
#define KEY_ENTER 13
#define KEY_SPACE 32
#define KEY_UP 72
#define KEY_DOWN 80
#define KEY_LEFT 75
#define KEY_RIGHT 77
#define KEY_CTRL_Z 26

// Game states
#define GAME_RUNNING 0
#define GAME_HUMAN_WIN 1
#define GAME_AI_WIN 2
#define GAME_DRAW 3
#define GAME_QUIT 4

//===============================================================================
// GAME INTERNAL CONSTANTS
//===============================================================================
#define GAME_DEPTH_LEVEL_EASY 2
#define GAME_DEPTH_LEVEL_MEDIUM 4
#define GAME_DEPTH_LEVEL_HARD 6

#define GAME_DEPTH_LEVEL_MAX 10
#define GAME_DEPTH_LEVEL_WARN 7

//===============================================================================
// MAIN MINIMAX EVALUATION FUNCTIONS
//===============================================================================

// Description: Minimax with last move.
//
// Parameters:
//   board: 2D array representing the game board
//   depth: Maximum search depth
//   alpha: Alpha value for alpha-beta pruning
//   beta: Beta value for alpha-beta pruning
int minimax_with_last_move(int **board, int depth, int alpha, int beta,
        int maximizing_player, int ai_player, int last_x,
        int last_y);

// Description: Display rules.
//
// Parameters:
//   None
//
// Returns:
//   None
void display_rules();

/**
 * Main evaluation function for minimax algorithm.
 * Evaluates the entire board position from the perspective of the given player.
 *
 * @param board 2D array representing the game board
 * @param size Size of the board (typically 15 or 19)
 * @param player The player to evaluate for (AI_CELL_CROSSES or AI_CELL_NAUGHTS)
 * @return Score where positive values favor the player, negative favor opponent
 */
int evaluate_position(int **board, int size, int player);

/**
 * Fast incremental evaluation function for minimax algorithm.
 * Only evaluates positions near the last move for better performance.
 *
 * @param board 2D array representing the game board
 * @param size Size of the board (typically 15 or 19)
 * @param player The player to evaluate for (AI_CELL_CROSSES or AI_CELL_NAUGHTS)
 * @param last_x X coordinate of the last move
 * @param last_y Y coordinate of the last move
 * @return Score where positive values favor the player, negative favor opponent
 */
int evaluate_position_incremental(int **board, int size, int player, int last_x,
        int last_y);

/**
 * Checks if the specified player has won the game (5 in a row).
 *
 * @param board 2D array representing the game board
 * @param size Size of the board
 * @param player The player to check for (AI_CELL_CROSSES or AI_CELL_NAUGHTS)
 * @return 1 if player has won, 0 otherwise
 */
int has_winner(int **board, int size, int player);

/**
 * Example minimax implementation using the evaluation function.
 * This shows how to integrate the evaluation with minimax + alpha-beta pruning.
 *
 * @param board 2D array representing the game board
 * @param size Size of the board
 * @param depth Maximum search depth
 * @param alpha Alpha value for alpha-beta pruning
 * @param beta Beta value for alpha-beta pruning
 * @param maximizing_player 1 if maximizing player's turn, 0 for minimizing
 * @param ai_player The AI player (AI_CELL_CROSSES or AI_CELL_NAUGHTS)
 * @return Best evaluation score found
 */
int minimax_example(int **board, int size, int depth, int alpha, int beta,
        int maximizing_player, int ai_player);

//===============================================================================
// PATTERN ANALYSIS FUNCTIONS
//===============================================================================

/**
 * Calculates the threat score for a stone at position (x,y).
 * Analyzes patterns in all four directions from the given position.
 *
 * @param board 2D array representing the game board
 * @param size Size of the board
 * @param player The player who owns the stone at (x,y)
 * @param x Row coordinate
 * @param y Column coordinate
 * @return Threat score for patterns around this position
 */
int calc_score_at(int **board, int size, int player, int x, int y);

/**
 * Analyzes a single line/direction for threat patterns.
 * The stone of interest is assumed to be at the center of the array.
 *
 * @param row 1D array representing stones in one direction
 * @param player The player to analyze threats for
 * @return Threat level (THREAT_* constant)
 */
int calc_threat_in_one_dimension(int *row, int player);

/**
 * Calculates additional score for combinations of threats.
 * For example, having both a three and a four creates a powerful combination.
 *
 * @param one First threat type
 * @param two Second threat type
 * @return Additional score for the combination
 */
int calc_combination_threat(int one, int two);

//===============================================================================
// UTILITY FUNCTIONS
//===============================================================================

/**
 * Returns the opponent of the given player.
 *
 * @param player Current player (AI_CELL_CROSSES or AI_CELL_NAUGHTS)
 * @return Opponent player
 */
int other_player(int player);

/**
 * Initializes the threat scoring matrix.
 * Must be called before using evaluation functions.
 */
void populate_threat_matrix(void);

/**
 * Resets a row array to OUT_OF_BOUNDS values.
 * Internal utility function for pattern analysis.
 *
 * @param row Array to reset
 * @param size Size of the array
 */
void reset_row(int *row, int size);

int calc_score_at(int **board, int size, int player, int x, int y);
int calc_threat_in_one_dimension(int *row, int player);
void populate_threat_matrix();

#endif // GOMOKU_H
