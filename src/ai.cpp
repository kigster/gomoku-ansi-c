//
//  ai.c
//  gomoku - AI module for minimax search and move finding
//
//  Handles AI move finding, minimax algorithm, and move prioritization
//

#include <iostream>
#include <algorithm>
#include <limits>
#include <cstring>
#include <ctime>
#include <cmath>
#include "ai.h"
#include "ansi.h"
#include "gomoku.hpp"

//===============================================================================
// AI CONSTANTS AND STRUCTURES
//===============================================================================

#define MAX_RADIUS 2
#define WIN_SCORE 1000000

//===============================================================================
// OPTIMIZED MOVE GENERATION
//===============================================================================

/**
 * Optimized move generation using cached interesting moves
 */
int generate_moves_optimized(game_state_t *game, move_t *moves, int current_player) {
    int move_count = 0;

    // Use cached interesting moves
    for (int i = 0; i < game->interesting_move_count; i++) {
        if (game->interesting_moves[i].is_active && 
                game->board[game->interesting_moves[i].x][game->interesting_moves[i].y] == static_cast<int>(gomoku::Player::Empty)) {

            moves[move_count].x = game->interesting_moves[i].x;
            moves[move_count].y = game->interesting_moves[i].y;
            moves[move_count].priority = get_move_priority_optimized(game, 
                    game->interesting_moves[i].x, game->interesting_moves[i].y, current_player);
            move_count++;
        }
    }

    return move_count;
}

/**
 * Optimized move prioritization that avoids expensive temporary placements
 */
int get_move_priority_optimized(game_state_t *game, int x, int y, int player) {
    int center = game->board_size / 2;
    int priority = 0;

    // Center bias - closer to center is better
    int center_dist = abs(x - center) + abs(y - center);
    priority += std::max(0, game->board_size - center_dist);

    // Quick threat evaluation without temporary placement
    int my_threat = evaluate_threat_fast(game->board, x, y, player, game->board_size);
    int opp_threat = evaluate_threat_fast(game->board, x, y, other_player(player), game->board_size);

    // Immediate win detection
    if (my_threat >= 100000) {
        return 100000;
    }

    // Blocking opponent's win
    if (opp_threat >= 100000) {
        return 50000;
    }

    // Killer move bonus
    if (is_killer_move(game, game->max_depth, x, y)) {
        priority += 10000;
    }

    // Prioritize offensive and defensive moves
    priority += my_threat / 10;   // Our opportunities
    priority += opp_threat / 5;   // Blocking opponent

    return priority;
}

/**
 * Fast threat evaluation without temporary board modifications
 */
int evaluate_threat_fast(int **board, int x, int y, int player, int board_size) {
    int max_threat = 0;

    // Check all 4 directions
    int directions[4][2] = {{1,0}, {0,1}, {1,1}, {1,-1}};

    for (int d = 0; d < 4; d++) {
        int dx = directions[d][0];
        int dy = directions[d][1];
        int count = 1; // Count the stone we're about to place

        // Count in positive direction
        int nx = x + dx, ny = y + dy;
        while (nx >= 0 && nx < board_size && ny >= 0 && ny < board_size && 
                board[nx][ny] == player) {
            count++;
            nx += dx;
            ny += dy;
        }

        // Count in negative direction
        nx = x - dx;
        ny = y - dy;
        while (nx >= 0 && nx < board_size && ny >= 0 && ny < board_size && 
                board[nx][ny] == player) {
            count++;
            nx -= dx;
            ny -= dy;
        }

        // Evaluate threat level
        int threat = 0;
        if (count >= 5) {
            threat = 100000; // Win
        } else if (count == 4) {
            threat = 10000;  // Strong threat
        } else if (count == 3) {
            threat = 1000;   // Medium threat
        } else if (count == 2) {
            threat = 100;    // Weak threat
        }

        max_threat = std::max(max_threat, threat);
    }

    return max_threat;
}

//===============================================================================
// MOVE EVALUATION AND ORDERING
//===============================================================================

int is_move_interesting(int **board, int x, int y, int stones_on_board, int board_size) {
    // If there are no stones on board, only center area is interesting
    if (stones_on_board == 0) {
        int center = board_size / 2;
        return (abs(x - center) <= 2 && abs(y - center) <= 2);
    }

    // Check if within 3 cells of any existing stone
    for (int i = std::max(0, x - MAX_RADIUS); i <= std::min(board_size - 1, x + MAX_RADIUS); i++) {
        for (int j = std::max(0, y - MAX_RADIUS); j <= std::min(board_size - 1, y + MAX_RADIUS); j++) {
            if (board[i][j] != static_cast<int>(gomoku::Player::Empty)) {
                return 1; // Found a stone within 3 cells
            }
        }
    }

    return 0; // No stones nearby, not interesting
}

int is_winning_move(int **board, int x, int y, int player, int board_size) {
    board[x][y] = player;
    int is_win = has_winner(board, board_size, player);
    board[x][y] = static_cast<int>(gomoku::Player::Empty);
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
    priority += std::max(0, board_size - center_dist);

    // Check for immediate threats/opportunities
    board[x][y] = player; // Temporarily place the move
    int my_score = calc_score_at(board, board_size, player, x, y);
    board[x][y] = other_player(player); // Check opponent's response
    int opp_score = calc_score_at(board, board_size, other_player(player), x, y);
    board[x][y] = static_cast<int>(gomoku::Player::Empty); // Restore empty

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

    // Compute position hash
    uint64_t hash = compute_zobrist_hash(game);

    // Probe transposition table
    int tt_value;
    if (probe_transposition(game, hash, depth, alpha, beta, &tt_value)) {
        return tt_value;
    }

    // Check for immediate wins/losses first (terminal conditions)
    if (get_cached_winner(game, ai_player)) {
        int value = WIN_SCORE + depth; // Prefer faster wins
        store_transposition(game, hash, value, depth, TT_EXACT, -1, -1);
        return value;
    }
    if (get_cached_winner(game, other_player(ai_player))) {
        int value = -WIN_SCORE - depth; // Prefer slower losses
        store_transposition(game, hash, value, depth, TT_EXACT, -1, -1);
        return value;
    }

    // Check search depth limit
    if (depth == 0) {
        int value = evaluate_position_incremental(board, game->board_size, ai_player, last_x, last_y);
        store_transposition(game, hash, value, depth, TT_EXACT, -1, -1);
        return value;
    }

    // Use cached stone count
    if (game->stones_on_board == 0) {
        return 0; // Draw
    }

    int current_player_turn = maximizing_player ? ai_player : other_player(ai_player);

    // Generate and sort moves using optimized method
    move_t moves[361]; // Max for 19x19 board
    int move_count = generate_moves_optimized(game, moves, current_player_turn);

    if (move_count == 0) {
        return 0; // No moves available
    }

    // Sort moves by priority (best first)
    qsort(moves, move_count, sizeof(move_t), compare_moves);

    int best_x = -1, best_y = -1;
    int original_alpha = alpha;

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

            // Aggressive pruning: Skip moves with very low priority at deeper levels
            if (depth > 2 && moves[m].priority < 10) {
                continue;
            }

            board[i][j] = current_player_turn;

            // Update hash incrementally
            int player_index = (current_player_turn == static_cast<int>(gomoku::Player::Cross)) ? 0 : 1;
            int pos = i * game->board_size + j;
            game->current_hash ^= game->zobrist_keys[player_index][pos];

            // Temporary cache invalidation
            invalidate_winner_cache(game);

            int eval = minimax_with_timeout(game, board, depth - 1, alpha, beta, 0, ai_player, i, j);

            // Restore hash
            game->current_hash ^= game->zobrist_keys[player_index][pos];

            // Restore cache
            invalidate_winner_cache(game);

            board[i][j] = static_cast<int>(gomoku::Player::Empty);

            if (eval > max_eval) {
                max_eval = eval;
                best_x = i;
                best_y = j;
            }
            alpha = std::max(alpha, eval);

            // Early termination for winning moves
            if (eval >= WIN_SCORE - 1000) {
                break;
            }

            if (beta <= alpha) {
                break; // Alpha-beta pruning
            }
        }

        // Store in transposition table
        int flag = (max_eval <= original_alpha) ? TT_UPPER_BOUND : 
            (max_eval >= beta) ? TT_LOWER_BOUND : TT_EXACT;
        store_transposition(game, hash, max_eval, depth, flag, best_x, best_y);

        // Store killer move if beta cutoff occurred
        if (max_eval >= beta && best_x != -1) {
            store_killer_move(game, depth, best_x, best_y);
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

            // Aggressive pruning: Skip moves with very low priority at deeper levels
            if (depth > 2 && moves[m].priority < 10) {
                continue;
            }

            board[i][j] = current_player_turn;

            // Update hash incrementally
            int player_index = (current_player_turn == static_cast<int>(gomoku::Player::Cross)) ? 0 : 1;
            int pos = i * game->board_size + j;
            game->current_hash ^= game->zobrist_keys[player_index][pos];

            // Temporary cache invalidation
            invalidate_winner_cache(game);

            int eval = minimax_with_timeout(game, board, depth - 1, alpha, beta, 1, ai_player, i, j);

            // Restore hash
            game->current_hash ^= game->zobrist_keys[player_index][pos];

            // Restore cache
            invalidate_winner_cache(game);

            board[i][j] = static_cast<int>(gomoku::Player::Empty);

            if (eval < min_eval) {
                min_eval = eval;
                best_x = i;
                best_y = j;
            }
            beta = std::min(beta, eval);

            // Early termination for losing moves
            if (eval <= -WIN_SCORE + 1000) {
                break;
            }

            if (beta <= alpha) {
                break; // Alpha-beta pruning
            }
        }

        // Store in transposition table
        int flag = (min_eval <= original_alpha) ? TT_UPPER_BOUND : 
            (min_eval >= beta) ? TT_LOWER_BOUND : TT_EXACT;
        store_transposition(game, hash, min_eval, depth, flag, best_x, best_y);

        // Store killer move if alpha cutoff occurred
        if (min_eval <= alpha && best_x != -1) {
            store_killer_move(game, depth, best_x, best_y);
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
            if (game->board[i][j] == static_cast<int>(gomoku::Player::Cross)) {
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
                        game->board[new_x][new_y] == static_cast<int>(gomoku::Player::Empty)) {
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
        *best_x = std::max(0, std::min(game->board_size - 1, *best_x));
        *best_y = std::max(0, std::min(game->board_size - 1, *best_y));
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
            if (game->board[i][j] != static_cast<int>(gomoku::Player::Empty)) {
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

    // Generate and sort moves using optimized method
    move_t moves[361]; // Max for 19x19 board
    int move_count = generate_moves_optimized(game, moves, static_cast<int>(gomoku::Player::Naught));

    // Check for immediate winning moves first
    for (int i = 0; i < move_count; i++) {
        if (evaluate_threat_fast(game->board, moves[i].x, moves[i].y, static_cast<int>(gomoku::Player::Naught), game->board_size) >= 100000) {
            *best_x = moves[i].x;
            *best_y = moves[i].y;
            snprintf(game->ai_status_message, sizeof(game->ai_status_message), 
                    "%s%s%s It's a checkmate ;-)", 
                    COLOR_BLUE, "O", COLOR_RESET);
            add_ai_history_entry(game, 1); // Only checked 1 move
            return;
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

    // Iterative deepening search
    for (int current_depth = 1; current_depth <= game->max_depth; current_depth++) {
        if (is_search_timed_out(game)) {
            break;
        }

        int depth_best_score = -WIN_SCORE - 1;
        int depth_best_x = *best_x;
        int depth_best_y = *best_y;

        // Search all moves at current depth
        for (int m = 0; m < move_count; m++) {
            // Check for timeout before evaluating each move
            if (is_search_timed_out(game)) {
                game->search_timed_out = 1;
                break;
            }

            int i = moves[m].x;
            int j = moves[m].y;

            game->board[i][j] = static_cast<int>(gomoku::Player::Naught);

            // Update hash incrementally
            int player_index = 1; // static_cast<int>(gomoku::Player::Naught)
            int pos = i * game->board_size + j;
            game->current_hash ^= game->zobrist_keys[player_index][pos];

            int score = minimax_with_timeout(game, game->board, current_depth - 1, -WIN_SCORE - 1, WIN_SCORE + 1,
                    0, static_cast<int>(gomoku::Player::Naught), i, j);

            // Restore hash
            game->current_hash ^= game->zobrist_keys[player_index][pos];

            game->board[i][j] = static_cast<int>(gomoku::Player::Empty);

            if (score > depth_best_score) {
                depth_best_score = score;
                depth_best_x = i;
                depth_best_y = j;

                // Early termination for very good moves
                if (score >= WIN_SCORE - 1000) {
                    snprintf(game->ai_status_message, sizeof(game->ai_status_message), 
                            "%s%s%s Win (depth %d, %d moves).", 
                            COLOR_BLUE, "O", COLOR_RESET, current_depth, moves_considered + 1);
                    *best_x = depth_best_x;
                    *best_y = depth_best_y;
                    add_ai_history_entry(game, moves_considered + 1);
                    return; // Exit function early
                }
            }

            moves_considered++;
            if (current_depth == game->max_depth) {
                printf("%s•%s", COLOR_BLUE, COLOR_RESET);
                fflush(stdout);
            }

            // Break if timeout occurred during minimax search
            if (game->search_timed_out) {
                break;
            }
        }

        // If we completed this depth without timeout, use the result
        if (!game->search_timed_out) {
            *best_x = depth_best_x;
            *best_y = depth_best_y;
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
