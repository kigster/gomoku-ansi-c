//
//  ai.h
//  gomoku - AI module for minimax search and move finding
//
//  Handles AI move finding, minimax algorithm, and move prioritization
//

#ifndef AI_H
#define AI_H

#include "gomoku.hpp"
#include "game.h"

//===============================================================================
// AI MOVE FINDING FUNCTIONS
//===============================================================================

/**
 * Finds the best AI move using minimax algorithm with alpha-beta pruning.
 * 
 * @param game The game state
 * @param best_x Pointer to store the best x coordinate
 * @param best_y Pointer to store the best y coordinate
 */
void find_best_ai_move(game_state_t *game, int *best_x, int *best_y);

/**
 * Finds the AI's first move (random placement near human's first move).
 * 
 * @param game The game state
 * @param best_x Pointer to store the best x coordinate
 * @param best_y Pointer to store the best y coordinate
 */
void find_first_ai_move(game_state_t *game, int *best_x, int *best_y);

//===============================================================================
// MINIMAX ALGORITHM
//===============================================================================

/**
 * Minimax algorithm with alpha-beta pruning and timeout support.
 * 
 * @param game The game state
 * @param board The game board
 * @param depth Current search depth
 * @param alpha Alpha value for pruning
 * @param beta Beta value for pruning
 * @param maximizing_player 1 if maximizing, 0 if minimizing
 * @param ai_player The AI player
 * @param last_x X coordinate of last move
 * @param last_y Y coordinate of last move
 * @return Best evaluation score
 */
int minimax_with_timeout(game_state_t *game, int **board, int depth, int alpha, int beta,
        int maximizing_player, int ai_player, int last_x, int last_y);

/**
 * Wrapper for backward compatibility with existing minimax function.
 * 
 * @param board The game board
 * @param depth Current search depth
 * @param alpha Alpha value for pruning
 * @param beta Beta value for pruning
 * @param maximizing_player 1 if maximizing, 0 if minimizing
 * @param ai_player The AI player
 * @return Best evaluation score
 */
int minimax(int **board, int depth, int alpha, int beta, int maximizing_player, int ai_player);

//===============================================================================
// MOVE EVALUATION AND ORDERING
//===============================================================================

/**
 * Structure to hold move coordinates and priority for sorting.
 */
typedef struct {
    int x, y;
    int priority;
} move_t;

/**
 * Optimized move generation using cached interesting moves.
 * 
 * @param game The game state
 * @param moves Array to store generated moves
 * @param current_player The current player
 * @return Number of moves generated
 */
int generate_moves_optimized(game_state_t *game, move_t *moves, int current_player);

/**
 * Optimized move prioritization that avoids expensive temporary placements.
 * 
 * @param game The game state
 * @param x Row coordinate
 * @param y Column coordinate
 * @param player The player making the move
 * @return Priority score for the move
 */
int get_move_priority_optimized(game_state_t *game, int x, int y, int player);

/**
 * Fast threat evaluation without temporary board modifications.
 * 
 * @param board The game board
 * @param x Row coordinate
 * @param y Column coordinate
 * @param player The player making the move
 * @param board_size Size of the board
 * @return Threat score for the move
 */
int evaluate_threat_fast(int **board, int x, int y, int player, int board_size);

/**
 * Checks if a move position is "interesting" (within range of existing stones).
 * 
 * @param board The game board
 * @param x Row coordinate
 * @param y Column coordinate
 * @param stones_on_board Number of stones currently on board
 * @param board_size Size of the board
 * @return 1 if interesting, 0 otherwise
 */
int is_move_interesting(int **board, int x, int y, int stones_on_board, int board_size);

/**
 * Checks if a move results in an immediate win.
 * 
 * @param board The game board
 * @param x Row coordinate
 * @param y Column coordinate
 * @param player The player making the move
 * @param board_size Size of the board
 * @return 1 if winning move, 0 otherwise
 */
int is_winning_move(int **board, int x, int y, int player, int board_size);

/**
 * Calculates move priority for ordering (higher = better).
 * 
 * @param board The game board
 * @param x Row coordinate
 * @param y Column coordinate
 * @param player The player making the move
 * @param board_size Size of the board
 * @return Priority score for the move
 */
int get_move_priority(int **board, int x, int y, int player, int board_size);

/**
 * Comparison function for sorting moves by priority (descending).
 * 
 * @param a First move
 * @param b Second move
 * @return Comparison result
 */
int compare_moves(const void *a, const void *b);

#endif // AI_H 
