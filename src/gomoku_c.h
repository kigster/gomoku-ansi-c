//
//  gomoku_c.h
//  gomoku - C-compatible header for legacy C code
//
//  Provides C interface while transitioning to C++23
//

#ifndef GOMOKU_C_H
#define GOMOKU_C_H

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

//===============================================================================
// GAME CONSTANTS (C-compatible)
//===============================================================================

#define GAME_NAME "Gomoku"
#define GAME_VERSION "2.0.0"
#define GAME_AUTHOR "Konstantin Gredeskoul"
#define GAME_LICENSE "MIT License"
#define GAME_URL "https://github.com/kigster/gomoku-ansi-c"
#define GAME_DESCRIPTION "Gomoku, also known as Five in a Row"
#define GAME_COPYRIGHT "© 2025 Konstantin Gredeskoul, MIT License"

#define GAME_RULES_BRIEF \
    " ↑ ↓ ← → (arrows) ───→ to move around, \n" \
    "  Enter or Space   ───→ to make a move, \n" \
    "  U                ───→ to undo last move pair (if --undo is enabled), \n" \
    "  ?                ───→ to show game rules, \n" \
    "  ESC              ───→ to quit game."

#define GAME_RULES_LONG \
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
// UNICODE DISPLAY CONSTANTS
//===============================================================================

// Unicode characters for board display
#define UNICODE_EMPTY "·"
#define UNICODE_CROSSES "✕"
#define UNICODE_NAUGHTS "○"
#define UNICODE_CURSOR "✕"
#define UNICODE_OCCUPIED "\033[0;33m◼︎"
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
// FUNCTION DECLARATIONS
//===============================================================================

// Evaluation functions
int evaluate_position(int **board, int size, int player);
int evaluate_position_incremental(int **board, int size, int player, int last_x, int last_y);
int has_winner(int **board, int size, int player);
int calc_score_at(int **board, int size, int player, int x, int y);
int calc_threat_in_one_dimension(int *row, int player);
int calc_combination_threat(int one, int two);
int other_player(int player);
void reset_row(int *row, int size);
void populate_threat_matrix(void);
void display_rules(void);

// Minimax functions
int minimax_with_last_move(int **board, int depth, int alpha, int beta,
                          int maximizing_player, int ai_player, int last_x, int last_y);
int minimax_example(int **board, int size, int depth, int alpha, int beta,
                   int maximizing_player, int ai_player);

// Board management functions
int **create_board(int size);
void free_board(int **board, int size);
int is_valid_move(int **board, int x, int y, int size);
const char* get_coordinate_unicode(int index);
int board_to_display_coord(int board_coord);
int display_to_board_coord(int display_coord);

#ifdef __cplusplus
}
#endif

#endif // GOMOKU_C_H