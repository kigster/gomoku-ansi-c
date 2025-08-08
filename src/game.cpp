//
//  game.c
//  gomoku - Game logic, state management, and move history
//
//  Handles game state, move validation, history management, and timing
//

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include "game.h"
#include "ai.h"
#include "gomoku.hpp"

//===============================================================================
// GAME INITIALIZATION AND CLEANUP
//===============================================================================

game_state_t *init_game(cli_config_t config) {
    game_state_t *game = static_cast<game_state_t*>(std::malloc(sizeof(game_state_t)));
    if (!game) {
        return NULL;
    }

    // Initialize board
    game->board = create_board(config.board_size);
    if (!game->board) {
        free(game);
        return NULL;
    }

    // Initialize game parameters
    game->board_size = config.board_size;
    game->cursor_x = config.board_size / 2;
    game->cursor_y = config.board_size / 2;
    game->current_player = static_cast<int>(gomoku::Player::Cross); // Human plays first
    game->game_state = static_cast<int>(gomoku::GameState::Running);
    game->max_depth = config.max_depth;
    game->move_timeout = config.move_timeout;
    game->config = config;

    // Initialize history
    game->move_history_count = 0;
    game->ai_history_count = 0;
    memset(game->ai_status_message, 0, sizeof(game->ai_status_message));

    // Initialize AI move tracking
    game->last_ai_move_x = -1;
    game->last_ai_move_y = -1;

    // Initialize timing
    game->total_human_time = 0.0;
    game->total_ai_time = 0.0;
    game->move_start_time = 0.0;
    game->search_start_time = 0.0;
    game->search_timed_out = 0;

    // Initialize optimization caches
    init_optimization_caches(game);

    // Initialize transposition table
    init_transposition_table(game);

    // Initialize killer moves
    init_killer_moves(game);

    return game;
}

void cleanup_game(game_state_t *game) {
    if (game) {
        if (game->board) {
            free_board(game->board, game->board_size);
        }
        free(game);
    }
}

//===============================================================================
// GAME LOGIC FUNCTIONS
//===============================================================================

void check_game_state(game_state_t *game) {
    if (has_winner(game->board, game->board_size, static_cast<int>(gomoku::Player::Cross))) {
        game->game_state = static_cast<int>(gomoku::GameState::HumanWin);
    } else if (has_winner(game->board, game->board_size, static_cast<int>(gomoku::Player::Naught))) {
        game->game_state = static_cast<int>(gomoku::GameState::AIWin);
    } else {
        // Check for draw (board full)
        int empty_cells = 0;
        for (int i = 0; i < game->board_size; i++) {
            for (int j = 0; j < game->board_size; j++) {
                if (game->board[i][j] == static_cast<int>(gomoku::Player::Empty)) {
                    empty_cells++;
                }
            }
        }
        if (empty_cells == 0) {
            game->game_state = static_cast<int>(gomoku::GameState::Draw);
        }
    }
}

int make_move(game_state_t *game, int x, int y, int player, double time_taken, int positions_evaluated) {
    if (!is_valid_move(game->board, x, y, game->board_size)) {
        return 0;
    }

    // Record move in history before placing it
    add_move_to_history(game, x, y, player, time_taken, positions_evaluated);

    // Make the move
    game->board[x][y] = player;

    // Update optimization caches
    update_interesting_moves(game, x, y);

    // Check for game end conditions
    check_game_state(game);

    // Switch to next player if game is still running
    if (game->game_state == static_cast<int>(gomoku::GameState::Running)) {
        game->current_player = other_player(game->current_player);
    }

    return 1;
}

int can_undo(game_state_t *game) {
    // Need at least 2 moves to undo (human + AI)
    return game->config.enable_undo && game->move_history_count >= 2;
}

void undo_last_moves(game_state_t *game) {
    if (!can_undo(game)) {
        return;
    }

    // Remove last two moves (AI move and human move) and update timing totals
    for (int i = 0; i < 2; i++) {
        if (game->move_history_count > 0) {
            game->move_history_count--;
            move_history_t last_move = game->move_history[game->move_history_count];
            game->board[last_move.x][last_move.y] = static_cast<int>(gomoku::Player::Empty);

            // Subtract time from totals
            if (last_move.player == static_cast<int>(gomoku::Player::Cross)) {
                game->total_human_time -= last_move.time_taken;
            } else {
                game->total_ai_time -= last_move.time_taken;
            }
        }
    }

    // Remove last AI thinking entry
    if (game->ai_history_count > 0) {
        game->ai_history_count--;
    }

    // Reset AI last move highlighting
    game->last_ai_move_x = -1;
    game->last_ai_move_y = -1;

    // Reset to human turn (since we removed AI move last)
    game->current_player = static_cast<int>(gomoku::Player::Cross);

    // Clear AI status message
    strcpy(game->ai_status_message, "");

    // Reset game state to running (in case it was won)
    game->game_state = static_cast<int>(gomoku::GameState::Running);
}

//===============================================================================
// TIMING FUNCTIONS
//===============================================================================

double get_current_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

void start_move_timer(game_state_t *game) {
    game->move_start_time = get_current_time();
}

double end_move_timer(game_state_t *game) {
    double end_time = get_current_time();
    return end_time - game->move_start_time;
}

int is_search_timed_out(game_state_t *game) {
    if (game->move_timeout <= 0) {
        return 0; // No timeout set
    }

    double elapsed = get_current_time() - game->search_start_time;
    return elapsed >= game->move_timeout;
}

//===============================================================================
// HISTORY MANAGEMENT
//===============================================================================

void add_move_to_history(game_state_t *game, int x, int y, int player, double time_taken, int positions_evaluated) {
    if (game->move_history_count < MAX_MOVE_HISTORY) {
        move_history_t *move = &game->move_history[game->move_history_count];
        move->x = x;
        move->y = y;
        move->player = player;
        move->time_taken = time_taken;
        move->positions_evaluated = positions_evaluated;
        game->move_history_count++;

        // Add to total time for each player
        if (player == static_cast<int>(gomoku::Player::Cross)) {
            game->total_human_time += time_taken;
        } else {
            game->total_ai_time += time_taken;
        }
    }
}

void add_ai_history_entry(game_state_t *game, int moves_evaluated) {
    if (game->ai_history_count >= MAX_AI_HISTORY) {
        // Shift history up to make room
        for (int i = 0; i < MAX_AI_HISTORY - 1; i++) {
            strcpy(game->ai_history[i], game->ai_history[i + 1]);
        }
        game->ai_history_count = MAX_AI_HISTORY - 1;
    }

    snprintf(game->ai_history[game->ai_history_count], sizeof(game->ai_history[game->ai_history_count]),
            "%2d | %3d positions evaluated", game->ai_history_count + 1, moves_evaluated);
    game->ai_history_count++;
} 

//===============================================================================
// OPTIMIZATION FUNCTIONS
//===============================================================================

void init_optimization_caches(game_state_t *game) {
    // Initialize interesting moves cache
    game->interesting_move_count = 0;
    game->stones_on_board = 0;
    game->winner_cache_valid = 0;
    game->has_winner_cache[0] = 0;
    game->has_winner_cache[1] = 0;

    // If board is empty, only center area is interesting
    if (game->stones_on_board == 0) {
        int center = game->board_size / 2;
        int count = 0;
        for (int i = center - 2; i <= center + 2; i++) {
            for (int j = center - 2; j <= center + 2; j++) {
                if (i >= 0 && i < game->board_size && j >= 0 && j < game->board_size) {
                    game->interesting_moves[count].x = i;
                    game->interesting_moves[count].y = j;
                    game->interesting_moves[count].is_active = 1;
                    count++;
                }
            }
        }
        game->interesting_move_count = count;
    }

    // Initialize transposition table
    init_transposition_table(game);

    // Initialize killer moves
    init_killer_moves(game);

    // Initialize advanced optimizations from research papers
    init_threat_space_search(game);
    init_aspiration_windows(game);
}

void update_interesting_moves(game_state_t *game, int x, int y) {
    // Update stone count
    game->stones_on_board++;

    // Invalidate winner cache
    invalidate_winner_cache(game);

    // Add new interesting moves around the placed stone
    const int radius = 2; // MAX_RADIUS

    for (int i = std::max(0, x - radius); i <= std::min(game->board_size - 1, x + radius); i++) {
        for (int j = std::max(0, y - radius); j <= std::min(game->board_size - 1, y + radius); j++) {
            if (game->board[i][j] == static_cast<int>(gomoku::Player::Empty)) {
                // Check if this position is already in the interesting moves
                int found = 0;
                for (int k = 0; k < game->interesting_move_count; k++) {
                    if (game->interesting_moves[k].x == i && game->interesting_moves[k].y == j && 
                            game->interesting_moves[k].is_active) {
                        found = 1;
                        break;
                    }
                }

                if (!found && game->interesting_move_count < 361) {
                    game->interesting_moves[game->interesting_move_count].x = i;
                    game->interesting_moves[game->interesting_move_count].y = j;
                    game->interesting_moves[game->interesting_move_count].is_active = 1;
                    game->interesting_move_count++;
                }
            }
        }
    }

    // Remove the move that was just played from interesting moves
    for (int k = 0; k < game->interesting_move_count; k++) {
        if (game->interesting_moves[k].x == x && game->interesting_moves[k].y == y) {
            game->interesting_moves[k].is_active = 0;
            break;
        }
    }
}

void invalidate_winner_cache(game_state_t *game) {
    game->winner_cache_valid = 0;
}

int get_cached_winner(game_state_t *game, int player) {
    if (!game->winner_cache_valid) {
        // Compute winner status for both players
        game->has_winner_cache[0] = has_winner(game->board, game->board_size, static_cast<int>(gomoku::Player::Cross));
        game->has_winner_cache[1] = has_winner(game->board, game->board_size, static_cast<int>(gomoku::Player::Naught));
        game->winner_cache_valid = 1;
    }

    if (player == static_cast<int>(gomoku::Player::Cross)) {
        return game->has_winner_cache[0];
    } else {
        return game->has_winner_cache[1];
    }
}

//===============================================================================
// TRANSPOSITION TABLE FUNCTIONS
//===============================================================================

void init_transposition_table(game_state_t *game) {
    // Initialize transposition table
    memset(game->transposition_table, 0, sizeof(game->transposition_table));

    // Initialize Zobrist keys with random values
    srand(12345); // Use fixed seed for reproducible results
    for (int player = 0; player < 2; player++) {
        for (int pos = 0; pos < 361; pos++) {
            game->zobrist_keys[player][pos] = 
                ((uint64_t)rand() << 32) | rand();
        }
    }

    // Compute initial hash
    game->current_hash = compute_zobrist_hash(game);
}

uint64_t compute_zobrist_hash(game_state_t *game) {
    uint64_t hash = 0;

    for (int i = 0; i < game->board_size; i++) {
        for (int j = 0; j < game->board_size; j++) {
            if (game->board[i][j] != static_cast<int>(gomoku::Player::Empty)) {
                int player_index = (game->board[i][j] == static_cast<int>(gomoku::Player::Cross)) ? 0 : 1;
                int pos = i * game->board_size + j;
                hash ^= game->zobrist_keys[player_index][pos];
            }
        }
    }

    return hash;
}

void store_transposition(game_state_t *game, uint64_t hash, int value, int depth, int flag, int best_x, int best_y) {
    int index = hash % TRANSPOSITION_TABLE_SIZE;
    transposition_entry_t *entry = &game->transposition_table[index];

    // Replace if this entry is deeper or empty
    if (entry->hash == 0 || entry->depth <= depth) {
        entry->hash = hash;
        entry->value = value;
        entry->depth = depth;
        entry->flag = flag;
        entry->best_move_x = best_x;
        entry->best_move_y = best_y;
    }
}

int probe_transposition(game_state_t *game, uint64_t hash, int depth, int alpha, int beta, int *value) {
    int index = hash % TRANSPOSITION_TABLE_SIZE;
    transposition_entry_t *entry = &game->transposition_table[index];

    if (entry->hash == hash && entry->depth >= depth) {
        *value = entry->value;

        if (entry->flag == TT_EXACT) {
            return 1; // Exact value
        } else if (entry->flag == TT_LOWER_BOUND && entry->value >= beta) {
            return 1; // Beta cutoff
        } else if (entry->flag == TT_UPPER_BOUND && entry->value <= alpha) {
            return 1; // Alpha cutoff
        }
    }

    return 0; // Not found or not usable
}

void init_killer_moves(game_state_t *game) {
    // Initialize killer moves table
    for (int depth = 0; depth < MAX_SEARCH_DEPTH; depth++) {
        for (int move_num = 0; move_num < MAX_KILLER_MOVES; move_num++) {
            game->killer_moves[depth][move_num][0] = -1;
            game->killer_moves[depth][move_num][1] = -1;
        }
    }
}

void store_killer_move(game_state_t *game, int depth, int x, int y) {
    if (depth >= MAX_SEARCH_DEPTH) return;

    // Don't store if already a killer move
    if (is_killer_move(game, depth, x, y)) return;

    // Shift killer moves and insert new one at the front
    for (int i = MAX_KILLER_MOVES - 1; i > 0; i--) {
        game->killer_moves[depth][i][0] = game->killer_moves[depth][i-1][0];
        game->killer_moves[depth][i][1] = game->killer_moves[depth][i-1][1];
    }

    game->killer_moves[depth][0][0] = x;
    game->killer_moves[depth][0][1] = y;
}

int is_killer_move(game_state_t *game, int depth, int x, int y) {
    if (depth >= MAX_SEARCH_DEPTH) return 0;

    for (int i = 0; i < MAX_KILLER_MOVES; i++) {
        if (game->killer_moves[depth][i][0] == x && 
                game->killer_moves[depth][i][1] == y) {
            return 1;
        }
    }
    return 0;
} 

//===============================================================================
// ADVANCED OPTIMIZATION FUNCTIONS (FROM RESEARCH PAPERS)
//===============================================================================

void init_threat_space_search(game_state_t *game) {
    game->threat_count = 0;
    game->use_aspiration_windows = 1;
    game->null_move_allowed = 1;
    game->null_move_count = 0;

    // Initialize threat array
    for (int i = 0; i < MAX_THREATS; i++) {
        game->active_threats[i].is_active = 0;
    }
}

void update_threat_analysis(game_state_t *game, int x, int y, int player) {
    // Invalidate nearby threats
    for (int i = 0; i < game->threat_count; i++) {
        if (game->active_threats[i].is_active) {
            int dx = abs(game->active_threats[i].x - x);
            int dy = abs(game->active_threats[i].y - y);
            if (dx <= 2 && dy <= 2) {
                game->active_threats[i].is_active = 0;
            }
        }
    }

    // Analyze new threats created by this move
    const int radius = 4;
    for (int i = std::max(0, x - radius); i <= std::min(game->board_size - 1, x + radius); i++) {
        for (int j = std::max(0, y - radius); j <= std::min(game->board_size - 1, y + radius); j++) {
            if (game->board[i][j] == static_cast<int>(gomoku::Player::Empty)) {
                // Check if this position creates a threat
                int threat_level = evaluate_threat_fast(game->board, i, j, player, game->board_size);
                if (threat_level > 100 && game->threat_count < MAX_THREATS) {
                    game->active_threats[game->threat_count].x = i;
                    game->active_threats[game->threat_count].y = j;
                    game->active_threats[game->threat_count].threat_type = threat_level;
                    game->active_threats[game->threat_count].player = player;
                    game->active_threats[game->threat_count].priority = threat_level;
                    game->active_threats[game->threat_count].is_active = 1;
                    game->threat_count++;
                }
            }
        }
    }
}

// Threat-space search integrated into AI move generation

void init_aspiration_windows(game_state_t *game) {
    for (int depth = 0; depth < MAX_SEARCH_DEPTH; depth++) {
        game->aspiration_windows[depth].alpha = -gomoku::WIN_SCORE;
        game->aspiration_windows[depth].beta = gomoku::WIN_SCORE;
        game->aspiration_windows[depth].depth = depth;
    }
}

int get_aspiration_window(game_state_t *game, int depth, int *alpha, int *beta) {
    if (!game->use_aspiration_windows || depth >= MAX_SEARCH_DEPTH) {
        *alpha = -gomoku::WIN_SCORE;
        *beta = gomoku::WIN_SCORE;
        return 0;
    }

    *alpha = game->aspiration_windows[depth].alpha;
    *beta = game->aspiration_windows[depth].beta;
    return 1;
}

void update_aspiration_window(game_state_t *game, int depth, int value, int alpha, int beta) {
    if (depth >= MAX_SEARCH_DEPTH) return;

    // Update the window for future searches at this depth
    game->aspiration_windows[depth].alpha = std::max(alpha, value - ASPIRATION_WINDOW);
    game->aspiration_windows[depth].beta = std::min(beta, value + ASPIRATION_WINDOW);
}

int should_try_null_move(game_state_t *game, int depth) {
    // Don't try null move if:
    // - Not allowed
    // - Already tried too many null moves
    // - Depth too low
    // - In endgame
    return game->null_move_allowed && 
        game->null_move_count < 2 && 
        depth >= 3 && 
        game->stones_on_board < (game->board_size * game->board_size) / 2;
}

int try_null_move_pruning(game_state_t *game, int depth, int beta, int ai_player) {
    if (!should_try_null_move(game, depth)) {
        return 0;
    }

    // Temporarily disable null moves to avoid infinite recursion
    game->null_move_allowed = 0;
    game->null_move_count++;

    // Search with reduced depth
    int null_score = -minimax_with_timeout(game, game->board, depth - NULL_MOVE_REDUCTION - 1, 
            -(beta + 1), -beta, 0, ai_player, -1, -1);

    // Restore null move settings
    game->null_move_allowed = 1;
    game->null_move_count--;

    // If null move fails high, we can prune
    if (null_score >= beta) {
        return beta; // Null move cutoff
    }

    return 0; // No pruning
} 
