//
//  game.c
//  gomoku - Game logic, state management, and move history
//
//  Handles game state, move validation, history management, and timing
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "game.h"
#include "ai.h"
#include "json.h"

// Helper to create a JSON number string with exactly 3 decimal places for milliseconds
static json_object *json_ms_from_seconds(double seconds) {
    char buf[32];
    double ms = round(seconds * 1000000.0) / 1000.0;  // Round to microseconds
    snprintf(buf, sizeof(buf), "%.3f", ms);
    return json_object_new_double_s(atof(buf), buf);
}

uint64_t compute_zobrist_hash(game_state_t *game);
void invalidate_winner_cache(game_state_t *game);
static void rebuild_optimization_caches(game_state_t *game);



//===============================================================================
// GAME INITIALIZATION AND CLEANUP
//===============================================================================

game_state_t *init_game(cli_config_t config) {
    game_state_t *game = malloc(sizeof(game_state_t));
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
    game->current_player = AI_CELL_CROSSES; // X always plays first
    game->game_state = GAME_RUNNING;
    game->max_depth = config.max_depth;
    game->move_timeout = config.move_timeout;
    game->search_radius = config.search_radius;
    game->replay_mode = 0;
    game->config = config;

    // Initialize player types (X=CROSSES=1, O=NAUGHTS=-1)
    // Map X/O configuration to CROSSES/NAUGHTS
    game->player_type[0] = config.player_x_type;  // CROSSES (X, plays first)
    game->player_type[1] = config.player_o_type;  // NAUGHTS (O, plays second)

    // Initialize depths
    game->depth_for_player[0] = (config.depth_x >= 0) ? config.depth_x : config.max_depth;
    game->depth_for_player[1] = (config.depth_o >= 0) ? config.depth_o : config.max_depth;

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
    if (has_winner(game->board, game->board_size, AI_CELL_CROSSES)) {
        game->game_state = GAME_HUMAN_WIN;
    } else if (has_winner(game->board, game->board_size, AI_CELL_NAUGHTS)) {
        game->game_state = GAME_AI_WIN;
    } else {
        // Check for draw (board full)
        int empty_cells = 0;
        for (int i = 0; i < game->board_size; i++) {
            for (int j = 0; j < game->board_size; j++) {
                if (game->board[i][j] == AI_CELL_EMPTY) {
                    empty_cells++;
                }
            }
        }
        if (empty_cells == 0) {
            game->game_state = GAME_DRAW;
        }
    }
}

int make_move(game_state_t *game, int x, int y, int player, double time_taken, int positions_evaluated, int own_score, int opponent_score) {
    if (!is_valid_move(game->board, x, y, game->board_size)) {
        return 0;
    }

    // Record move in history before placing it
    if (game->move_history_count < MAX_MOVE_HISTORY) {
        move_history_t *move = &game->move_history[game->move_history_count];
        move->x = x;
        move->y = y;
        move->player = player;
        move->time_taken = time_taken;
        move->positions_evaluated = positions_evaluated;
        move->own_score = own_score;
        move->opponent_score = opponent_score;
        move->is_winner = 0;  // Will be set after checking game state
        game->move_history_count++;

        // Add to total time for each player
        // Note: CROSSES (X) time goes to human_time, NAUGHTS (O) time goes to ai_time
        // This convention is used for backwards compatibility with original code
        if (player == AI_CELL_CROSSES) {
            game->total_human_time += time_taken;
        } else {
            game->total_ai_time += time_taken;
        }
    }

    // Make the move
    game->board[x][y] = player;

    // Update optimization caches
    update_interesting_moves(game, x, y);

    // Check for game end conditions
    check_game_state(game);

    // Mark winning move (GAME_HUMAN_WIN = X won, GAME_AI_WIN = O won)
    if (game->game_state == GAME_HUMAN_WIN || game->game_state == GAME_AI_WIN) {
        if (game->move_history_count > 0) {
            game->move_history[game->move_history_count - 1].is_winner = 1;
        }
    }

    // Switch to next player if game is still running
    if (game->game_state == GAME_RUNNING) {
        game->current_player = other_player(game->current_player);
    }

    return 1;
}

int can_undo(game_state_t *game) {
    if (!game->config.enable_undo) {
        return 0;
    }

    // In Human vs Human mode, allow undo with at least 1 move
    if (game->player_type[0] == PLAYER_TYPE_HUMAN &&
        game->player_type[1] == PLAYER_TYPE_HUMAN) {
        return game->move_history_count >= 1;
    }

    // In modes with AI, need at least 2 moves to undo a turn pair
    return game->move_history_count >= 2;
}

void undo_last_moves(game_state_t *game) {
    if (!can_undo(game)) {
        return;
    }

    // Determine how many moves to undo
    int moves_to_undo = 2;  // Default: undo a pair (for AI modes)

    // In Human vs Human mode, only undo the last move
    if (game->player_type[0] == PLAYER_TYPE_HUMAN &&
        game->player_type[1] == PLAYER_TYPE_HUMAN) {
        moves_to_undo = 1;
    }

    // Make sure we don't undo more moves than we have
    if (moves_to_undo > game->move_history_count) {
        moves_to_undo = game->move_history_count;
    }

    // Track if we're undoing any AI moves (for AI history cleanup)
    int ai_moves_undone = 0;

    // Remove last move(s) and update timing totals
    for (int i = 0; i < moves_to_undo; i++) {
        if (game->move_history_count > 0) {
            game->move_history_count--;
            move_history_t last_move = game->move_history[game->move_history_count];
            game->board[last_move.x][last_move.y] = AI_CELL_EMPTY;

            // Check if this move was from an AI player
            int player_index = (last_move.player == AI_CELL_CROSSES) ? 0 : 1;
            if (game->player_type[player_index] == PLAYER_TYPE_AI) {
                ai_moves_undone++;
            }

            // Subtract time from totals
            if (last_move.player == AI_CELL_CROSSES) {
                game->total_human_time -= last_move.time_taken;
            } else {
                game->total_ai_time -= last_move.time_taken;
            }
        }
    }

    // Remove AI thinking entries for each AI move undone
    for (int i = 0; i < ai_moves_undone && game->ai_history_count > 0; i++) {
        game->ai_history_count--;
    }

    // Reset AI last move highlighting
    game->last_ai_move_x = -1;
    game->last_ai_move_y = -1;

    // Set current player to whoever should move next
    if (game->move_history_count > 0) {
        move_history_t last_move = game->move_history[game->move_history_count - 1];
        game->current_player = other_player(last_move.player);
    } else {
        game->current_player = AI_CELL_CROSSES;  // Start of game, X plays first
    }

    // Clear AI status message
    strcpy(game->ai_status_message, "");

    // Reset game state to running (in case it was won)
    game->game_state = GAME_RUNNING;


    rebuild_optimization_caches(game);
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
        if (player == AI_CELL_CROSSES) {
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


static void rebuild_optimization_caches(game_state_t *game) {
    int size = game->board_size;

    game->stones_on_board = 0;
    game->interesting_move_count = 0;
    memset(game->interesting_moves, 0, sizeof(game->interesting_moves));

    unsigned char candidate[19][19];
    memset(candidate, 0, sizeof(candidate));

    for (int x = 0; x < size; x++) {
        for (int y = 0; y < size; y++) {
            if (game->board[x][y] == AI_CELL_EMPTY) {
                continue;
            }
            game->stones_on_board++;

            for (int dx = -3; dx <= 3; dx++) {
                for (int dy = -3; dy <= 3; dy++) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx < 0 || nx >= size || ny < 0 || ny >= size) {
                        continue;
                    }
                    if (game->board[nx][ny] != AI_CELL_EMPTY) {
                        continue;
                    }
                    candidate[nx][ny] = 1;
                }
            }
        }
    }

    for (int x = 0; x < size; x++) {
        for (int y = 0; y < size; y++) {
            if (!candidate[x][y]) {
                continue;
            }
            game->interesting_moves[game->interesting_move_count].x = x;
            game->interesting_moves[game->interesting_move_count].y = y;
            game->interesting_moves[game->interesting_move_count].is_active = 1;
            game->interesting_move_count++;
        }
    }

    invalidate_winner_cache(game);
    game->current_hash = compute_zobrist_hash(game);
}

void update_interesting_moves(game_state_t *game, int x, int y) {
    // Update stone count
    game->stones_on_board++;

    // Update zobrist hash incrementally
    int cell = game->board[x][y];
    if (cell != AI_CELL_EMPTY) {
        int player_index = (cell == AI_CELL_CROSSES) ? 0 : 1;
        int pos = x * game->board_size + y;
        game->current_hash ^= game->zobrist_keys[player_index][pos];
    }

    // Invalidate winner cache
    invalidate_winner_cache(game);

    // Add new interesting moves around the placed stone
    const int radius = 2; // MAX_RADIUS

    for (int i = max(0, x - radius); i <= min(game->board_size - 1, x + radius); i++) {
        for (int j = max(0, y - radius); j <= min(game->board_size - 1, y + radius); j++) {
            if (game->board[i][j] == AI_CELL_EMPTY) {
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
        game->has_winner_cache[0] = has_winner(game->board, game->board_size, AI_CELL_CROSSES);
        game->has_winner_cache[1] = has_winner(game->board, game->board_size, AI_CELL_NAUGHTS);
        game->winner_cache_valid = 1;
    }

    if (player == AI_CELL_CROSSES) {
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

    // Initialize Zobrist keys using a linear congruential generator with fixed seed
    // This avoids calling srand() which would overwrite the time-based seed used
    // for game randomization (e.g., random move selection among equal-weight moves)
    uint64_t lcg_state = 12345ULL;
    for (int player = 0; player < 2; player++) {
        for (int pos = 0; pos < 361; pos++) {
            // LCG: next = (a * state + c) mod m
            // Using parameters from Numerical Recipes
            lcg_state = lcg_state * 6364136223846793005ULL + 1442695040888963407ULL;
            uint64_t high = lcg_state;
            lcg_state = lcg_state * 6364136223846793005ULL + 1442695040888963407ULL;
            uint64_t low = lcg_state;
            game->zobrist_keys[player][pos] = (high & 0xFFFFFFFF00000000ULL) | (low >> 32);
        }
    }

    // Compute initial hash
    game->current_hash = compute_zobrist_hash(game);
}

uint64_t compute_zobrist_hash(game_state_t *game) {
    uint64_t hash = 0;

    for (int i = 0; i < game->board_size; i++) {
        for (int j = 0; j < game->board_size; j++) {
            if (game->board[i][j] != AI_CELL_EMPTY) {
                int player_index = (game->board[i][j] == AI_CELL_CROSSES) ? 0 : 1;
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
    for (int i = max(0, x - radius); i <= min(game->board_size - 1, x + radius); i++) {
        for (int j = max(0, y - radius); j <= min(game->board_size - 1, y + radius); j++) {
            if (game->board[i][j] == AI_CELL_EMPTY) {
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
        game->aspiration_windows[depth].alpha = -WIN_SCORE;
        game->aspiration_windows[depth].beta = WIN_SCORE;
        game->aspiration_windows[depth].depth = depth;
    }
}

int get_aspiration_window(game_state_t *game, int depth, int *alpha, int *beta) {
    if (!game->use_aspiration_windows || depth >= MAX_SEARCH_DEPTH) {
        *alpha = -WIN_SCORE;
        *beta = WIN_SCORE;
        return 0;
    }

    *alpha = game->aspiration_windows[depth].alpha;
    *beta = game->aspiration_windows[depth].beta;
    return 1;
}

void update_aspiration_window(game_state_t *game, int depth, int value, int alpha, int beta) {
    if (depth >= MAX_SEARCH_DEPTH) return;

    // Update the window for future searches at this depth
    game->aspiration_windows[depth].alpha = max(alpha, value - ASPIRATION_WINDOW);
    game->aspiration_windows[depth].beta = min(beta, value + ASPIRATION_WINDOW);
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

//===============================================================================
// JSON EXPORT
//===============================================================================

int write_game_json(game_state_t *game, const char *filename) {
    if (!filename || strlen(filename) == 0) {
        return 0;
    }

    json_object *root = json_object_new_object();
    if (!root) {
        return 0;
    }

    // Player X configuration
    json_object *player_x = json_object_new_object();
    json_object_object_add(player_x, "player",
        json_object_new_string(game->player_type[0] == PLAYER_TYPE_HUMAN ? "human" : "AI"));
    if (game->player_type[0] == PLAYER_TYPE_AI) {
        json_object_object_add(player_x, "depth",
            json_object_new_int(game->depth_for_player[0]));
    }
    // Total time in milliseconds with 3 decimal places (microsecond precision)
    json_object_object_add(player_x, "time_ms",
        json_ms_from_seconds(game->total_human_time));
    json_object_object_add(root, "X", player_x);

    // Player O configuration
    json_object *player_o = json_object_new_object();
    json_object_object_add(player_o, "player",
        json_object_new_string(game->player_type[1] == PLAYER_TYPE_HUMAN ? "human" : "AI"));
    if (game->player_type[1] == PLAYER_TYPE_AI) {
        json_object_object_add(player_o, "depth",
            json_object_new_int(game->depth_for_player[1]));
    }
    // Total time in milliseconds with 3 decimal places (microsecond precision)
    json_object_object_add(player_o, "time_ms",
        json_ms_from_seconds(game->total_ai_time));
    json_object_object_add(root, "O", player_o);

    // Game parameters
    json_object_object_add(root, "board", json_object_new_int(game->board_size));
    json_object_object_add(root, "radius", json_object_new_int(game->search_radius));

    if (game->move_timeout > 0) {
        json_object_object_add(root, "timeout", json_object_new_int(game->move_timeout));
    } else {
        json_object_object_add(root, "timeout", json_object_new_string("none"));
    }
    json_object_object_add(root, "undo",
        json_object_new_string(game->config.enable_undo ? "on" : "off"));

    // Winner (GAME_HUMAN_WIN = X won, GAME_AI_WIN = O won)
    const char *winner_str = "none";
    if (game->game_state == GAME_HUMAN_WIN) {
        winner_str = "X";
    } else if (game->game_state == GAME_AI_WIN) {
        winner_str = "O";
    } else if (game->game_state == GAME_DRAW) {
        winner_str = "draw";
    }
    json_object_object_add(root, "winner", json_object_new_string(winner_str));

    // Final board state as array of row strings (space-separated)
    json_object *board_array = json_object_new_array();
    const char *x_symbol = "✕";
    const char *o_symbol = "○";
    int board_size = game->board_size;
    size_t x_len = strlen(x_symbol);
    size_t o_len = strlen(o_symbol);
    size_t max_sym_len = (x_len > o_len) ? x_len : o_len;
    if (max_sym_len < 1) {
        max_sym_len = 1;
    }
    size_t row_len = (board_size > 0) ? (board_size * max_sym_len + (size_t)(board_size - 1)) : 0;
    char *row_str = malloc(row_len + 1);
    if (row_str) {
        for (int row = 0; row < board_size; row++) {
            int idx = 0;
            for (int col = 0; col < board_size; col++) {
                int cell = game->board[row][col];
                if (cell == AI_CELL_CROSSES) {
                    memcpy(&row_str[idx], x_symbol, x_len);
                    idx += (int)x_len;
                } else if (cell == AI_CELL_NAUGHTS) {
                    memcpy(&row_str[idx], o_symbol, o_len);
                    idx += (int)o_len;
                } else {
                    row_str[idx++] = '.';
                }
                if (col < board_size - 1) {
                    row_str[idx++] = ' ';
                }
            }
            row_str[idx] = '\0';
            json_object_array_add(board_array, json_object_new_string(row_str));
        }
        free(row_str);
    }
    json_object_object_add(root, "board_state", board_array);

    // Moves array
    json_object *moves_array = json_object_new_array();

    for (int i = 0; i < game->move_history_count; i++) {
        move_history_t *move = &game->move_history[i];
        json_object *move_obj = json_object_new_object();

        // Player identifier
        const char *player_name;
        int player_index = (move->player == AI_CELL_CROSSES) ? 0 : 1;
        int is_ai = (game->player_type[player_index] == PLAYER_TYPE_AI);

        if (move->player == AI_CELL_CROSSES) {
            player_name = is_ai ? "X (AI)" : "X (human)";
        } else {
            player_name = is_ai ? "O (AI)" : "O (human)";
        }

        // Position array [x, y]
        json_object *pos_array = json_object_new_array();
        json_object_array_add(pos_array, json_object_new_int(move->x));
        json_object_array_add(pos_array, json_object_new_int(move->y));
        json_object_object_add(move_obj, player_name, pos_array);

        // AI-specific fields
        if (is_ai && move->positions_evaluated > 0) {
            json_object_object_add(move_obj, "moves_searched",
                json_object_new_int(move->positions_evaluated));
        }
        if (is_ai && move->own_score != 0) {
            json_object_object_add(move_obj, "score",
                json_object_new_int(move->own_score));
        }
        if (is_ai && move->opponent_score != 0) {
            json_object_object_add(move_obj, "opponent",
                json_object_new_int(move->opponent_score));
        }

        // Time taken in milliseconds (3 decimal places = microsecond precision)
        json_object_object_add(move_obj, "time_ms",
            json_ms_from_seconds(move->time_taken));

        // Winner flag
        if (move->is_winner) {
            json_object_object_add(move_obj, "winner", json_object_new_boolean(1));
        }

        json_object_array_add(moves_array, move_obj);
    }

    json_object_object_add(root, "moves", moves_array);

    // Write to file
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        json_object_put(root);
        return 0;
    }

    const char *json_str = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    fprintf(fp, "%s\n", json_str);
    fclose(fp);

    json_object_put(root);
    return 1;
}

int load_game_json(const char *filename, replay_data_t *data) {
    if (!filename || !data) {
        return 0;
    }

    // Initialize data
    memset(data, 0, sizeof(replay_data_t));
    strcpy(data->winner, "none");

    // Read and parse JSON file
    json_object *root = json_object_from_file(filename);
    if (!root) {
        return 0;
    }

    // Get board size
    json_object *board_obj;
    if (json_object_object_get_ex(root, "board", &board_obj)) {
        data->board_size = json_object_get_int(board_obj);
    } else {
        data->board_size = 19;  // Default
    }

    // Get winner
    json_object *winner_obj;
    if (json_object_object_get_ex(root, "winner", &winner_obj)) {
        const char *winner_str = json_object_get_string(winner_obj);
        if (winner_str) {
            strncpy(data->winner, winner_str, sizeof(data->winner) - 1);
        }
    }

    // Get moves array
    json_object *moves_obj;
    if (!json_object_object_get_ex(root, "moves", &moves_obj)) {
        json_object_put(root);
        return 0;
    }

    int num_moves = json_object_array_length(moves_obj);
    if (num_moves > MAX_MOVE_HISTORY) {
        num_moves = MAX_MOVE_HISTORY;
    }

    data->move_count = 0;
    for (int i = 0; i < num_moves; i++) {
        json_object *move_obj = json_object_array_get_idx(moves_obj, i);
        if (!move_obj) continue;

        move_history_t *move = &data->moves[data->move_count];
        memset(move, 0, sizeof(move_history_t));

        // The move object has a key like "X (AI)" or "O (human)" with position array
        json_object_object_foreach(move_obj, key, val) {
            // Check for position array (the player key)
            if (json_object_is_type(val, json_type_array) && json_object_array_length(val) == 2) {
                move->x = json_object_get_int(json_object_array_get_idx(val, 0));
                move->y = json_object_get_int(json_object_array_get_idx(val, 1));

                // Determine player from key
                if (key[0] == 'X') {
                    move->player = AI_CELL_CROSSES;
                } else if (key[0] == 'O') {
                    move->player = AI_CELL_NAUGHTS;
                }
            }
            // Get time_ms
            else if (strcmp(key, "time_ms") == 0) {
                move->time_taken = json_object_get_double(val) / 1000.0;  // Convert ms to seconds
            }
            // Get positions evaluated
            else if (strcmp(key, "moves_searched") == 0) {
                move->positions_evaluated = json_object_get_int(val);
            }
            // Get own score
            else if (strcmp(key, "score") == 0) {
                move->own_score = json_object_get_int(val);
            }
            // Get opponent score
            else if (strcmp(key, "opponent") == 0) {
                move->opponent_score = json_object_get_int(val);
            }
            // Get winner flag
            else if (strcmp(key, "winner") == 0 && json_object_is_type(val, json_type_boolean)) {
                move->is_winner = json_object_get_boolean(val);
            }
        }

        data->move_count++;
    }

    json_object_put(root);
    return 1;
}
