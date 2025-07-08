//
//  ai.c
//  gomoku - AI module for minimax search and move finding
//
//  Handles AI move finding, minimax algorithm, and move prioritization
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ai.h"

//===============================================================================
// MOVE EVALUATION AND ORDERING
//===============================================================================

#define MAX_RADIUS 2

int is_move_interesting(int **board, int x, int y, int stones_on_board, int board_size) {
    // If there are no stones on board, only center area is interesting
    if (stones_on_board == 0) {
        int center = board_size / 2;
        return (abs(x - center) <= 2 && abs(y - center) <= 2);
    }
    
    // Check if within 3 cells of any existing stone
    for (int i = max(0, x - MAX_RADIUS); i <= min(board_size - 1, x + MAX_RADIUS); i++) {
        for (int j = max(0, y - MAX_RADIUS); j <= min(board_size - 1, y + MAX_RADIUS); j++) {
            if (board[i][j] != AI_CELL_EMPTY) {
                return 1; // Found a stone within 3 cells
            }
        }
    }
    
    return 0; // No stones nearby, not interesting
}

int is_winning_move(int **board, int x, int y, int player, int board_size) {
    board[x][y] = player;
    int is_win = has_winner(board, board_size, player);
    board[x][y] = AI_CELL_EMPTY;
    return is_win;
}

int get_move_priority(int **board, int x, int y, int player, int board_size) {
    int center = board_size / 2;
    int priority = 0;
    
    // Immediate win gets highest priority
    if (is_winning_move(board, x, y, player, board_size)) {
        return 100000;
    }
    
    // Blocking opponent's win gets second highest priority
    if (is_winning_move(board, x, y, other_player(player), board_size)) {
        return 50000;
    }
    
    // Center bias - closer to center is better
    int center_dist = abs(x - center) + abs(y - center);
    priority += max(0, board_size - center_dist);
    
    // Check for immediate threats/opportunities
    board[x][y] = player; // Temporarily place the move
    int my_score = calc_score_at(board, board_size, player, x, y);
    board[x][y] = other_player(player); // Check opponent's response
    int opp_score = calc_score_at(board, board_size, other_player(player), x, y);
    board[x][y] = AI_CELL_EMPTY; // Restore empty
    
    // Prioritize offensive and defensive moves
    priority += my_score / 10;   // Our opportunities
    priority += opp_score / 5;   // Blocking opponent
    
    return priority;
}

int compare_moves(const void *a, const void *b) {
    move_t *move_a = (move_t *)a;
    move_t *move_b = (move_t *)b;
    return move_b->priority - move_a->priority; // Higher priority first
}

//===============================================================================
// MINIMAX ALGORITHM
//===============================================================================

int minimax(int **board, int depth, int alpha, int beta, int maximizing_player, int ai_player) {
    // Create a temporary game state to use the timeout version
    // This is for backward compatibility only
    game_state_t temp_game = {
        .board = board,
        .board_size = 19, // Default size
        .move_timeout = 0, // No timeout
        .search_timed_out = 0
    };
    
    // Use center position as default for initial call
    int center = 19 / 2;
    return minimax_with_timeout(&temp_game, board, depth, alpha, beta, maximizing_player, ai_player, center, center);
}

int minimax_with_timeout(game_state_t *game, int **board, int depth, int alpha, int beta,
                        int maximizing_player, int ai_player, int last_x, int last_y) {
    // Check for timeout first
    if (is_search_timed_out(game)) {
        game->search_timed_out = 1;
        return evaluate_position_incremental(board, game->board_size, ai_player, last_x, last_y);
    }

    // Check for immediate wins/losses first (terminal conditions)
    if (has_winner(board, game->board_size, ai_player)) {
        return WIN_SCORE + depth; // Prefer faster wins
    }
    if (has_winner(board, game->board_size, other_player(ai_player))) {
        return -WIN_SCORE - depth; // Prefer slower losses
    }
    
    // Check search depth limit
    if (depth == 0) {
        return evaluate_position_incremental(board, game->board_size, ai_player, last_x, last_y);
    }

    // Count stones once for this level
    int stones_on_board = 0;
    int moves_available = 0;
    for (int i = 0; i < game->board_size; i++) {
        for (int j = 0; j < game->board_size; j++) {
            if (board[i][j] == AI_CELL_EMPTY) {
                moves_available = 1;
            } else {
                stones_on_board++;
            }
        }
    }
    
    if (!moves_available) {
        return 0; // Draw
    }

    int current_player_turn = maximizing_player ? ai_player : other_player(ai_player);

    // Generate and sort moves for better alpha-beta pruning
    move_t moves[game->board_size * game->board_size];
    int move_count = 0;
    
    // Collect all interesting moves
    for (int i = 0; i < game->board_size; i++) {
        for (int j = 0; j < game->board_size; j++) {
            if (board[i][j] == AI_CELL_EMPTY && is_move_interesting(board, i, j, stones_on_board, game->board_size)) {
                moves[move_count].x = i;
                moves[move_count].y = j;
                moves[move_count].priority = get_move_priority(board, i, j, current_player_turn, game->board_size);
                move_count++;
            }
        }
    }
    
    // Sort moves by priority (best first)
    qsort(moves, move_count, sizeof(move_t), compare_moves);

    if (maximizing_player) {
        int max_eval = -WIN_SCORE - 1;

        for (int m = 0; m < move_count; m++) {
            // Check for timeout before evaluating each move
            if (is_search_timed_out(game)) {
                game->search_timed_out = 1;
                return max_eval;
            }
            
            int i = moves[m].x;
            int j = moves[m].y;
            
            board[i][j] = current_player_turn;
            int eval = minimax_with_timeout(game, board, depth - 1, alpha, beta, 0, ai_player, i, j);
            board[i][j] = AI_CELL_EMPTY;

            max_eval = max(max_eval, eval);
            alpha = max(alpha, eval);

            if (beta <= alpha) {
                return max_eval; // Alpha-beta pruning
            }
        }
        return max_eval;

    } else {
        int min_eval = WIN_SCORE + 1;

        for (int m = 0; m < move_count; m++) {
            // Check for timeout before evaluating each move
            if (is_search_timed_out(game)) {
                game->search_timed_out = 1;
                return min_eval;
            }
            
            int i = moves[m].x;
            int j = moves[m].y;
            
            board[i][j] = current_player_turn;
            int eval = minimax_with_timeout(game, board, depth - 1, alpha, beta, 1, ai_player, i, j);
            board[i][j] = AI_CELL_EMPTY;

            min_eval = min(min_eval, eval);
            beta = min(beta, eval);

            if (beta <= alpha) {
                return min_eval; // Alpha-beta pruning
            }
        }
        return min_eval;
    }
}

//===============================================================================
// AI MOVE FINDING FUNCTIONS
//===============================================================================

void find_first_ai_move(game_state_t *game, int *best_x, int *best_y) {
    // Find the human's first move
    int human_x = -1, human_y = -1;
    for (int i = 0; i < game->board_size && human_x == -1; i++) {
        for (int j = 0; j < game->board_size && human_x == -1; j++) {
            if (game->board[i][j] == AI_CELL_CROSSES) {
                human_x = i;
                human_y = j;
            }
        }
    }
    
    if (human_x == -1) {
        // Fallback: place in center if no human move found
        *best_x = game->board_size / 2;
        *best_y = game->board_size / 2;
        return;
    }
    
    // Collect valid positions 1-2 squares away from human move
    int valid_moves[50][2]; // Enough for nearby positions
    int move_count = 0;
    
    for (int distance = 1; distance <= 2; distance++) {
        for (int dx = -distance; dx <= distance; dx++) {
            for (int dy = -distance; dy <= distance; dy++) {
                if (dx == 0 && dy == 0) continue; // Skip the human's position
                
                int new_x = human_x + dx;
                int new_y = human_y + dy;
                
                // Check bounds and if position is empty
                if (new_x >= 0 && new_x < game->board_size && 
                    new_y >= 0 && new_y < game->board_size &&
                    game->board[new_x][new_y] == AI_CELL_EMPTY) {
                    valid_moves[move_count][0] = new_x;
                    valid_moves[move_count][1] = new_y;
                    move_count++;
                }
            }
        }
    }
    
    if (move_count > 0) {
        // Randomly select one of the valid moves
        int selected = rand() % move_count;
        *best_x = valid_moves[selected][0];
        *best_y = valid_moves[selected][1];
    } else {
        // Fallback: place adjacent to human move
        *best_x = human_x + (rand() % 3 - 1); // -1, 0, or 1
        *best_y = human_y + (rand() % 3 - 1);
        
        // Ensure bounds
        *best_x = max(0, min(game->board_size - 1, *best_x));
        *best_y = max(0, min(game->board_size - 1, *best_y));
    }
}

void find_best_ai_move(game_state_t *game, int *best_x, int *best_y) {
    // Initialize timeout tracking
    game->search_start_time = get_current_time();
    game->search_timed_out = 0;
    
    // Count stones on board to detect first AI move
    int stone_count = 0;
    for (int i = 0; i < game->board_size; i++) {
        for (int j = 0; j < game->board_size; j++) {
            if (game->board[i][j] != AI_CELL_EMPTY) {
                stone_count++;
            }
        }
    }
    
    // If there's exactly 1 stone (human's first move), use simple random placement
    if (stone_count == 1) {
        find_first_ai_move(game, best_x, best_y);
        add_ai_history_entry(game, 1); // Random placement, 1 "move" considered
        return;
    }
    
    // Regular minimax for subsequent moves
    int best_score = -WIN_SCORE - 1;
    *best_x = -1;
    *best_y = -1;

    // Clear previous AI status message and show thinking message
    strcpy(game->ai_status_message, "");
    if (game->move_timeout > 0) {
        printf("%s%s%s It's AI's Turn... Please wait... (timeout: %ds)\n", 
               COLOR_BLUE, "O", COLOR_RESET, game->move_timeout);
    } else {
        printf("%s%s%s It's AI's Turn... Please wait...\n", 
               COLOR_BLUE, "O", COLOR_RESET);
    }
    fflush(stdout);

    // Count stones once for move generation
    int stones_on_board = 0;
    for (int i = 0; i < game->board_size; i++) {
        for (int j = 0; j < game->board_size; j++) {
            if (game->board[i][j] != AI_CELL_EMPTY) {
                stones_on_board++;
            }
        }
    }

    // Check for immediate winning moves first
    for (int i = 0; i < game->board_size; i++) {
        for (int j = 0; j < game->board_size; j++) {
            if (game->board[i][j] == AI_CELL_EMPTY && 
                is_move_interesting(game->board, i, j, stones_on_board, game->board_size)) {
                if (is_winning_move(game->board, i, j, AI_CELL_NAUGHTS, game->board_size)) {
                    *best_x = i;
                    *best_y = j;
                    snprintf(game->ai_status_message, sizeof(game->ai_status_message), 
                             "%s%s%s It's a checkmate ;-)", 
                             COLOR_BLUE, "O", COLOR_RESET);
                    add_ai_history_entry(game, 1); // Only checked 1 move
                    return;
                }
            }
        }
    }

    // Generate and sort moves by priority
    move_t moves[game->board_size * game->board_size];
    int move_count = 0;
    
    for (int i = 0; i < game->board_size; i++) {
        for (int j = 0; j < game->board_size; j++) {
            if (game->board[i][j] == AI_CELL_EMPTY && 
                is_move_interesting(game->board, i, j, stones_on_board, game->board_size)) {
                moves[move_count].x = i;
                moves[move_count].y = j;
                moves[move_count].priority = get_move_priority(game->board, i, j, AI_CELL_NAUGHTS, game->board_size);
                move_count++;
            }
        }
    }
    
    // Sort moves by priority (best first)
    qsort(moves, move_count, sizeof(move_t), compare_moves);

    int moves_considered = 0;
    
    // Ensure we have a fallback move in case timeout occurs immediately
    if (move_count > 0 && *best_x == -1) {
        *best_x = moves[0].x;
        *best_y = moves[0].y;
    }
    
    for (int m = 0; m < move_count; m++) {
        // Check for timeout before evaluating each move
        if (is_search_timed_out(game)) {
            game->search_timed_out = 1;
            break;
        }
        
        int i = moves[m].x;
        int j = moves[m].y;
        
        game->board[i][j] = AI_CELL_NAUGHTS;
        int score = minimax_with_timeout(game, game->board, game->max_depth - 1, -WIN_SCORE - 1, WIN_SCORE + 1,
                                        0, AI_CELL_NAUGHTS, i, j);
        game->board[i][j] = AI_CELL_EMPTY;

        if (score > best_score) {
            best_score = score;
            *best_x = i;
            *best_y = j;
            
            // Early termination for very good moves
            if (score >= WIN_SCORE - 1000) {
                snprintf(game->ai_status_message, sizeof(game->ai_status_message), 
                         "%s%s%s Win (%d moves evaluated).", 
                         COLOR_BLUE, "O", COLOR_RESET, moves_considered + 1);
                add_ai_history_entry(game, moves_considered + 1);
                return; // Exit function early to avoid duplicate history entry
            }
        }

        moves_considered++;
        printf("%s•%s", COLOR_BLUE, COLOR_RESET);
        fflush(stdout);
        
        // Break if timeout occurred during minimax search
        if (game->search_timed_out) {
            break;
        }
    }
    
    // Store the completion message if not already set by early termination
    if (strlen(game->ai_status_message) == 0) {
        double elapsed = get_current_time() - game->search_start_time;
        if (game->search_timed_out) {
            snprintf(game->ai_status_message, sizeof(game->ai_status_message), 
                     "%.0fs timeout, checked %d moves", 
                     elapsed, moves_considered);
        } else {
            snprintf(game->ai_status_message, sizeof(game->ai_status_message), 
                     "Done in %.0fs (checked %d moves)", 
                     elapsed, moves_considered);
        }
    }
    
    // Add to AI history
    add_ai_history_entry(game, moves_considered);
} 