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
#include "game.h"

//===============================================================================
// GAME INITIALIZATION AND CLEANUP
//===============================================================================

game_state_t* init_game(int board_size, int max_depth, int move_timeout) {
    game_state_t *game = malloc(sizeof(game_state_t));
    if (!game) {
        return NULL;
    }
    
    // Initialize board
    game->board = create_board(board_size);
    if (!game->board) {
        free(game);
        return NULL;
    }
    
    // Initialize game parameters
    game->board_size = board_size;
    game->cursor_x = board_size / 2;
    game->cursor_y = board_size / 2;
    game->current_player = AI_CELL_BLACK; // Human plays first
    game->game_state = GAME_RUNNING;
    game->max_depth = max_depth;
    game->move_timeout = move_timeout;
    
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
    if (has_winner(game->board, game->board_size, AI_CELL_BLACK)) {
        game->game_state = GAME_HUMAN_WIN;
    } else if (has_winner(game->board, game->board_size, AI_CELL_WHITE)) {
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

int make_move(game_state_t *game, int x, int y, int player, double time_taken, int positions_evaluated) {
    if (!is_valid_move(game->board, x, y, game->board_size)) {
        return 0;
    }
    
    // Record move in history before placing it
    add_move_to_history(game, x, y, player, time_taken, positions_evaluated);
    
    // Make the move
    game->board[x][y] = player;
    
    // Check for game end conditions
    check_game_state(game);
    
    // Switch to next player if game is still running
    if (game->game_state == GAME_RUNNING) {
        game->current_player = other_player(game->current_player);
    }
    
    return 1;
}

int can_undo(game_state_t *game) {
    // Need at least 2 moves to undo (human + AI)
    return game->move_history_count >= 2;
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
            game->board[last_move.x][last_move.y] = AI_CELL_EMPTY;
            
            // Subtract time from totals
            if (last_move.player == AI_CELL_BLACK) {
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
    game->current_player = AI_CELL_BLACK;
    
    // Clear AI status message
    strcpy(game->ai_status_message, "");
    
    // Reset game state to running (in case it was won)
    game->game_state = GAME_RUNNING;
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
        if (player == AI_CELL_BLACK) {
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