//
//  game.h
//  gomoku - Game logic, state management, and move history
//
//  Handles game state, move validation, history management, and timing
//

#ifndef GAME_H
#define GAME_H

#include "gomoku.h"
#include "board.h"

//===============================================================================
// GAME CONSTANTS
//===============================================================================

#define MAX_MOVE_HISTORY 400
#define MAX_AI_HISTORY 20

//===============================================================================
// GAME STATE STRUCTURES
//===============================================================================

/**
 * Structure to represent a move in the game history
 */
typedef struct {
    int x, y;              // Position of the move
    int player;            // Player who made the move
    double time_taken;     // Time taken to make this move in seconds
    int positions_evaluated; // For AI moves, number of positions evaluated
} move_history_t;

/**
 * Structure to represent the current game state
 */
typedef struct {
    int **board;           // The game board
    int board_size;        // Size of the board
    int cursor_x, cursor_y; // Current cursor position
    int current_player;    // Current player (AI_CELL_BLACK or AI_CELL_WHITE)
    int game_state;        // Current game state (GAME_RUNNING, etc.)
    int max_depth;         // AI search depth
    int move_timeout;      // Move timeout in seconds (0 = no timeout)
    
    // Move history
    move_history_t move_history[MAX_MOVE_HISTORY];
    int move_history_count;
    
    // AI history
    char ai_history[MAX_AI_HISTORY][50];
    int ai_history_count;
    char ai_status_message[256];
    
    // Last AI move for highlighting
    int last_ai_move_x, last_ai_move_y;
    
    // Timing
    double total_human_time;
    double total_ai_time;
    double move_start_time;
    
    // Timeout tracking
    double search_start_time;
    int search_timed_out;
} game_state_t;

//===============================================================================
// GAME INITIALIZATION AND CLEANUP
//===============================================================================

/**
 * Initializes a new game state with the specified parameters.
 * 
 * @param board_size Size of the board (15 or 19)
 * @param max_depth AI search depth
 * @param move_timeout Timeout for moves in seconds (0 = no timeout)
 * @return Initialized game state, or NULL on failure
 */
game_state_t* init_game(int board_size, int max_depth, int move_timeout);

/**
 * Cleans up and frees game state resources.
 * 
 * @param game The game state to clean up
 */
void cleanup_game(game_state_t *game);

//===============================================================================
// GAME LOGIC FUNCTIONS
//===============================================================================

/**
 * Checks the current game state for win/draw conditions.
 * 
 * @param game The game state to check
 */
void check_game_state(game_state_t *game);

/**
 * Makes a move on the board and updates game state.
 * 
 * @param game The game state
 * @param x Row coordinate
 * @param y Column coordinate
 * @param player The player making the move
 * @param time_taken Time taken to make the move
 * @param positions_evaluated Number of positions evaluated (for AI moves)
 * @return 1 if move was successful, 0 if invalid
 */
int make_move(game_state_t *game, int x, int y, int player, double time_taken, int positions_evaluated);

/**
 * Checks if undo is possible (need at least 2 moves).
 * 
 * @param game The game state
 * @return 1 if undo is possible, 0 otherwise
 */
int can_undo(game_state_t *game);

/**
 * Undoes the last move pair (human + AI).
 * 
 * @param game The game state
 */
void undo_last_moves(game_state_t *game);

//===============================================================================
// TIMING FUNCTIONS
//===============================================================================

/**
 * Gets the current time in seconds.
 * 
 * @return Current time as double
 */
double get_current_time(void);

/**
 * Starts the move timer.
 * 
 * @param game The game state
 */
void start_move_timer(game_state_t *game);

/**
 * Ends the move timer and returns elapsed time.
 * 
 * @param game The game state
 * @return Elapsed time in seconds
 */
double end_move_timer(game_state_t *game);

/**
 * Checks if the search has timed out.
 * 
 * @param game The game state
 * @return 1 if timed out, 0 otherwise
 */
int is_search_timed_out(game_state_t *game);

//===============================================================================
// HISTORY MANAGEMENT
//===============================================================================

/**
 * Adds a move to the game history.
 * 
 * @param game The game state
 * @param x Row coordinate
 * @param y Column coordinate
 * @param player The player who made the move
 * @param time_taken Time taken to make the move
 * @param positions_evaluated Number of positions evaluated
 */
void add_move_to_history(game_state_t *game, int x, int y, int player, double time_taken, int positions_evaluated);

/**
 * Adds an AI thinking entry to the history.
 * 
 * @param game The game state
 * @param moves_evaluated Number of moves evaluated
 */
void add_ai_history_entry(game_state_t *game, int moves_evaluated);

#endif // GAME_H 