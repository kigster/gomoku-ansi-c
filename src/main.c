//
//  gomoku_main.c
//  gomoku - Main game loop with Unicode board and keyboard controls
//
//  UNIX/Linux/macOS version with interactive console interface
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#include "gomoku.h"

// Forward declarations
int minimax_with_last_move(int **board, int depth, int alpha, int beta, int maximizing_player,
                          int ai_player, int last_x, int last_y);
void display_rules();

//===============================================================================
// GAME CONSTANTS
//===============================================================================

#define BOARD_SIZE 19

// ANSI Color codes
#define COLOR_RESET       "\033[0m"
#define COLOR_BOLD_BLACK  "\033[1;30m"
#define COLOR_RED         "\033[31m"
#define COLOR_BLUE        "\033[34m"
#define COLOR_YELLOW      "\033[33m"

// Unicode characters for board display
#define UNICODE_EMPTY     "┼"
#define UNICODE_BLACK     "✕"
#define UNICODE_WHITE     "○"
#define UNICODE_CURSOR    "✕"
#define UNICODE_CORNER_TL "┌"
#define UNICODE_CORNER_TR "┐"
#define UNICODE_CORNER_BL "└"
#define UNICODE_CORNER_BR "┘"
#define UNICODE_EDGE_H    "─"
#define UNICODE_EDGE_V    "│"
#define UNICODE_T_TOP     "┬"
#define UNICODE_T_BOT     "┴"
#define UNICODE_T_LEFT    "├"
#define UNICODE_T_RIGHT   "┤"

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
// GLOBAL VARIABLES
//===============================================================================

static int **board;
static int cursor_x = 7, cursor_y = 7;     // Start at center
static int current_player = AI_CELL_BLACK; // Human plays black, goes first
static int game_state = GAME_RUNNING;
static int max_depth = 4;                  // Default difficulty (medium)
static char ai_status_message[256] = "";   // Store AI's last thinking result

// AI Deep Thinking History
#define MAX_AI_HISTORY 20
static char ai_history[MAX_AI_HISTORY][50];
static int ai_history_count = 0;

// Move History for UNDO functionality
#define MAX_MOVE_HISTORY 400  // More than enough for 19x19 board
typedef struct {
  int x, y;
  int player;
} move_history_t;
static move_history_t move_history[MAX_MOVE_HISTORY];
static int move_history_count = 0;

//===============================================================================
// KEYBOARD INPUT FUNCTIONS
//===============================================================================

struct termios original_termios;

void disable_raw_mode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
}

void enable_raw_mode() {
  tcgetattr(STDIN_FILENO, &original_termios);
  atexit(disable_raw_mode);

  struct termios raw = original_termios;
  raw.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int get_key() {
  char c;
  if (read(STDIN_FILENO, &c, 1) == 1) {
    if (c == '\033') { // ESC sequence
      char seq[3];
      if (read(STDIN_FILENO, &seq[0], 1) != 1)
        return c;
      if (read(STDIN_FILENO, &seq[1], 1) != 1)
        return c;

      if (seq[0] == '[') {
        switch (seq[1]) {
        case 'A':
          return KEY_UP;
        case 'B':
          return KEY_DOWN;
        case 'C':
          return KEY_RIGHT;
        case 'D':
          return KEY_LEFT;
        }
      }
    } else if (c == '\n' || c == '\r') {
      return KEY_ENTER;
    }
    return c;
  }
  return -1;
}

//===============================================================================
// BOARD MANAGEMENT
//===============================================================================

int **create_board() {
  int **new_board = malloc(BOARD_SIZE * sizeof(int *));
  for (int i = 0; i < BOARD_SIZE; i++) {
    new_board[i] = malloc(BOARD_SIZE * sizeof(int));
    for (int j = 0; j < BOARD_SIZE; j++) {
      new_board[i][j] = AI_CELL_EMPTY;
    }
  }
  return new_board;
}

void free_board(int **board) {
  for (int i = 0; i < BOARD_SIZE; i++) {
    free(board[i]);
  }
  free(board);
}

int is_valid_move(int x, int y) {
  return x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE &&
         board[x][y] == AI_CELL_EMPTY;
}

//===============================================================================
// DISPLAY FUNCTIONS
//===============================================================================

void clear_screen() {
  printf("\033[2J\033[H");
}

void add_move_to_history(int x, int y, int player) {
  if (move_history_count < MAX_MOVE_HISTORY) {
    move_history[move_history_count].x = x;
    move_history[move_history_count].y = y;
    move_history[move_history_count].player = player;
    move_history_count++;
  }
}

void add_ai_history_entry(int moves_evaluated) {
  if (ai_history_count >= MAX_AI_HISTORY) {
    // Shift history up to make room
    for (int i = 0; i < MAX_AI_HISTORY - 1; i++) {
      strcpy(ai_history[i], ai_history[i + 1]);
    }
    ai_history_count = MAX_AI_HISTORY - 1;
  }
  
  snprintf(ai_history[ai_history_count], sizeof(ai_history[ai_history_count]),
           "Move %d: %d moves", ai_history_count + 1, moves_evaluated);
  ai_history_count++;
}

int can_undo() {
  // Need at least 2 moves to undo (human + AI)
  return move_history_count >= 2;
}

void undo_last_moves() {
  if (!can_undo()) {
    return;
  }
  
  // Remove last two moves (AI move and human move)
  for (int i = 0; i < 2; i++) {
    if (move_history_count > 0) {
      move_history_count--;
      move_history_t last_move = move_history[move_history_count];
      board[last_move.x][last_move.y] = AI_CELL_EMPTY;
    }
  }
  
  // Remove last AI thinking entry
  if (ai_history_count > 0) {
    ai_history_count--;
  }
  
  // Reset to human turn (since we removed AI move last)
  current_player = AI_CELL_BLACK;
  
  // Clear AI status message
  strcpy(ai_status_message, "");
  
  // Reset game state to running (in case it was won)
  game_state = GAME_RUNNING;
}

void draw_game_header() {
  int board_width = BOARD_SIZE * 2 + 3; // Account for spacing and row numbers
  
  // Calculate centering
  const char* title = "GOMOKU";
  const char* copyright = "© 2025 Konstantin Gredeskoul";
  const char* description = "Game engine: clever evaluation functions + minimax with alpha-beta pruning";
  
  int title_padding = (board_width - strlen(title)) / 2;
  int copyright_padding = (board_width - strlen(copyright)) / 2;
  int desc_padding = (board_width - strlen(description)) / 2;
  
  printf("%s", COLOR_BOLD_BLACK);
  printf("%*s%s%*s\n", title_padding, "", title, title_padding, "");
  printf("%*s%s%*s\n", copyright_padding, "", copyright, copyright_padding, "");
  printf("%*s%s%*s\n", desc_padding, "", description, desc_padding, "");
  printf("%s", COLOR_RESET);
  printf("\n");
}

void draw_ai_history_sidebar(int start_row) {
  // Position cursor to the right of the board
  int sidebar_col = BOARD_SIZE * 2 + 8; // After board and some spacing
  
  // Draw AI Deep Thinking header
  printf("\033[%d;%dH%s%s%s", start_row, sidebar_col, COLOR_BLUE, "AI Deep Thinking", COLOR_RESET);
  printf("\033[%d;%dH%s", start_row + 1, sidebar_col, "================");
  
  // Draw history entries
  for (int i = 0; i < ai_history_count; i++) {
    printf("\033[%d;%dH%s", start_row + 2 + i, sidebar_col, ai_history[i]);
  }
}

void draw_board() {
  // Draw game header first
  draw_game_header();
  
  printf("  ");

  // Column numbers
  for (int j = 0; j < BOARD_SIZE; j++) {
    printf("%s%2d%s", COLOR_BOLD_BLACK, j, COLOR_RESET);
  }
  printf("\n");

  int board_start_row = 6; // After header (4 lines) + column numbers (1 line) + buffer (1 line)
  
  for (int i = 0; i < BOARD_SIZE; i++) {
    printf("%s%2d%s", COLOR_BOLD_BLACK, i, COLOR_RESET); // Row number

    for (int j = 0; j < BOARD_SIZE; j++) {
      // Show cursor position with yellow color
      if (i == cursor_x && j == cursor_y && board[i][j] == AI_CELL_EMPTY) {
        printf(" %s%s%s", COLOR_YELLOW, UNICODE_CURSOR, COLOR_RESET);
      } else {
        switch (board[i][j]) {
        case AI_CELL_BLACK:
          printf(" %s%s%s", COLOR_RED, UNICODE_BLACK, COLOR_RESET);
          break;
        case AI_CELL_WHITE:
          printf(" %s%s%s", COLOR_BLUE, UNICODE_WHITE, COLOR_RESET);
          break;
        default:
          printf(" %s%s%s", COLOR_BOLD_BLACK, UNICODE_EMPTY, COLOR_RESET);
          break;
        }
      }
    }
    printf("\n");
  }
  
  // Draw AI history sidebar
  draw_ai_history_sidebar(board_start_row);
}

void draw_status() {
  printf("\n");
  
  // Box width for the status border
  const int box_width = 50;
  
  // Top border
  printf("%s┌", COLOR_BOLD_BLACK);
  for (int i = 0; i < box_width - 2; i++) {
    printf("─");
  }
  printf("┐%s\n", COLOR_RESET);
  
  // Current Player
  if (current_player == AI_CELL_BLACK) {
    printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
           box_width - 4, "Current Player : ✕ Black(Human)", COLOR_BOLD_BLACK, COLOR_RESET);
  } else {
    printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
           box_width - 4, "Current Player : ○ White(AI)", COLOR_BOLD_BLACK, COLOR_RESET);
  }
  
  // Position
  char position_str[32];
  snprintf(position_str, sizeof(position_str), "Position : (%d, %d)", cursor_x, cursor_y);
  printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
         box_width - 4, position_str, COLOR_BOLD_BLACK, COLOR_RESET);
  
  // Difficulty
  const char* difficulty_name;
  switch (max_depth) {
    case 2: difficulty_name = "Easy"; break;
    case 4: difficulty_name = "Medium"; break;
    case 7: difficulty_name = "Hard"; break;
    default: difficulty_name = "Custom"; break;
  }
  char difficulty_str[32];
  snprintf(difficulty_str, sizeof(difficulty_str), "Difficulty : %s (depth %d)", difficulty_name, max_depth);
  printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
         box_width - 4, difficulty_str, COLOR_BOLD_BLACK, COLOR_RESET);
  
  // Separator line
  printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
         box_width - 4, "", COLOR_BOLD_BLACK, COLOR_RESET);
  
  // Controls header
  printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
         box_width - 4, "Controls :", COLOR_BOLD_BLACK, COLOR_RESET);
  
  // Control instructions
  printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
         box_width - 4, " Arrow Keys - Move cursor", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
         box_width - 4, " Space / Enter - Make move", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
         box_width - 4, " Ctrl-Z - Undo last move pair", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
         box_width - 4, " ? - Show game rules", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
         box_width - 4, " ESC - Quit game", COLOR_BOLD_BLACK, COLOR_RESET);

  // AI status message if available
  if (strlen(ai_status_message) > 0) {
    printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
           box_width - 4, "", COLOR_BOLD_BLACK, COLOR_RESET);
    
    // Extract clean message without ANSI color codes
    char clean_message[100] = "";
    const char* msg = ai_status_message;
    char* clean_ptr = clean_message;
    
    while (*msg && (clean_ptr - clean_message) < (int)(sizeof(clean_message) - 1)) {
      if (*msg == '\033') {
        // Skip ANSI escape sequence
        while (*msg && *msg != 'm') msg++;
        if (*msg) msg++;
      } else {
        *clean_ptr++ = *msg++;
      }
    }
    *clean_ptr = '\0';
    
    printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
           box_width - 4, clean_message, COLOR_BOLD_BLACK, COLOR_RESET);
  }

  // Game state messages
  if (game_state != GAME_RUNNING) {
    printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
           box_width - 4, "", COLOR_BOLD_BLACK, COLOR_RESET);
    
    switch (game_state) {
    case GAME_HUMAN_WIN:
      printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
             box_width - 4, "🎉 Human wins! ✕", COLOR_BOLD_BLACK, COLOR_RESET);
      break;
    case GAME_AI_WIN:
      printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
             box_width - 4, "🤖 AI wins! ○", COLOR_BOLD_BLACK, COLOR_RESET);
      break;
    case GAME_DRAW:
      printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
             box_width - 4, "🤝 Game is a draw!", COLOR_BOLD_BLACK, COLOR_RESET);
      break;
    }
    printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
           box_width - 4, "Press any key to exit...", COLOR_BOLD_BLACK, COLOR_RESET);
  }
  
  // Bottom border
  printf("%s└", COLOR_BOLD_BLACK);
  for (int i = 0; i < box_width - 2; i++) {
    printf("─");
  }
  printf("┘%s\n", COLOR_RESET);
}

void display_rules() {
  clear_screen();
  
  printf("%s═══════════════════════════════════════════════════════════════════════════════%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("%s                                 GOMOKU RULES                                   %s\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("%s═══════════════════════════════════════════════════════════════════════════════%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("\n");
  
  // Basic objective
  printf("%s🎯 OBJECTIVE%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("   Gomoku (Five in a Row) is a strategy game where players take turns placing\n");
  printf("   stones on a 19×19 board. The goal is to be the first to get five stones\n");
  printf("   in a row (horizontally, vertically, or diagonally).\n\n");
  
  // Game pieces
  printf("%s🎮 GAME PIECES%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("   %s%s%s Human Player (Black) - You play first\n", COLOR_RED, UNICODE_BLACK, COLOR_RESET);
  printf("   %s%s%s AI Player (White) - Computer opponent\n", COLOR_BLUE, UNICODE_WHITE, COLOR_RESET);
  printf("   %s%s%s Current cursor position\n\n", COLOR_YELLOW, UNICODE_CURSOR, COLOR_RESET);
  
  // How to play
  printf("%s📋 HOW TO PLAY%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("   1. Black (Human) always goes first\n");
  printf("   2. Players alternate turns placing one stone per turn\n");
  printf("   3. Stones are placed on intersections of the grid lines\n");
  printf("   4. Once placed, stones cannot be moved or removed\n");
  printf("   5. Use arrow keys to move cursor, Space/Enter to place stone\n\n");
  
  // Winning conditions
  printf("%s🏆 WINNING CONDITIONS%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("   Win by creating an unbroken line of exactly 5 stones:\n");
  printf("   • Horizontal: %s%s%s %s%s%s %s%s%s %s%s%s %s%s%s\n", 
         COLOR_RED, UNICODE_BLACK, COLOR_RESET,
         COLOR_RED, UNICODE_BLACK, COLOR_RESET,
         COLOR_RED, UNICODE_BLACK, COLOR_RESET,
         COLOR_RED, UNICODE_BLACK, COLOR_RESET,
         COLOR_RED, UNICODE_BLACK, COLOR_RESET);
  printf("   • Vertical:   Lines going up and down\n");
  printf("   • Diagonal:   Lines going diagonally in any direction\n");
  printf("   • Six or more stones in a row do NOT count as a win (overline rule)\n\n");
  
  // Basic strategies
  printf("%s🧠 BASIC STRATEGIES%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("   %sOffense & Defense:%s Balance creating your own lines with blocking\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("   opponent's attempts to get five in a row.\n\n");
  
  printf("   %sControl the Center:%s The center of the board provides more\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("   opportunities to create lines in multiple directions.\n\n");
  
  printf("   %sWatch for Threats:%s An 'open three' (three stones with both ends\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("   open) must be blocked immediately, or it becomes an unstoppable 'open four'.\n\n");
  
  printf("   %sCreate Forks:%s Try to create two threatening lines simultaneously\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("   - your opponent can only block one at a time!\n\n");
  
  // Game variations
  printf("%s⚙️  DIFFICULTY LEVELS%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("   • Easy (depth 2):   Quick moves, good for beginners\n");
  printf("   • Medium (depth 4): Balanced gameplay, default setting\n");
  printf("   • Hard (depth 7):   Advanced AI, challenging for experts\n\n");
  
  // Controls reminder
  printf("%s🎹 CONTROLS%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("   • Arrow Keys: Move cursor\n");
  printf("   • Space/Enter: Place stone\n");
  printf("   • Ctrl-Z: Undo last move pair (human + AI)\n");
  printf("   • ?: Show this help screen\n");
  printf("   • ESC: Quit game\n\n");
  
  printf("%s═══════════════════════════════════════════════════════════════════════════════%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("                      %sPress any key to return to game%s\n", COLOR_YELLOW, COLOR_RESET);
  printf("%s═══════════════════════════════════════════════════════════════════════════════%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
  
  // Wait for any key press
  get_key();
}

//===============================================================================
// AI FUNCTIONS
//===============================================================================

/**
 * Check if a move position is "interesting" - within 3 cells of any existing stone.
 * This optimization dramatically reduces the search space.
 * Pass stones_on_board to avoid recalculating for each move.
 */
int is_move_interesting(int **board, int x, int y, int stones_on_board) {
  // If there are no stones on board, only center area is interesting
  if (stones_on_board == 0) {
    int center = BOARD_SIZE / 2;
    return (abs(x - center) <= 2 && abs(y - center) <= 2);
  }
  
  // Check if within 3 cells of any existing stone
  for (int i = max(0, x - 3); i <= min(BOARD_SIZE - 1, x + 3); i++) {
    for (int j = max(0, y - 3); j <= min(BOARD_SIZE - 1, y + 3); j++) {
      if (board[i][j] != AI_CELL_EMPTY) {
        return 1; // Found a stone within 3 cells
      }
    }
  }
  
  return 0; // No stones nearby, not interesting
}

/**
 * Structure to hold move coordinates and priority for sorting
 */
typedef struct {
  int x, y;
  int priority;
} move_t;

/**
 * Check if a move results in an immediate win
 */
int is_winning_move(int **board, int x, int y, int player) {
  board[x][y] = player;
  int is_win = has_winner(board, BOARD_SIZE, player);
  board[x][y] = AI_CELL_EMPTY;
  return is_win;
}

/**
 * Calculate move priority for ordering (higher = better)
 */
int get_move_priority(int **board, int x, int y, int player) {
  int center = BOARD_SIZE / 2;
  int priority = 0;
  
  // Immediate win gets highest priority
  if (is_winning_move(board, x, y, player)) {
    return 100000;
  }
  
  // Blocking opponent's win gets second highest priority
  if (is_winning_move(board, x, y, other_player(player))) {
    return 50000;
  }
  
  // Center bias - closer to center is better
  int center_dist = abs(x - center) + abs(y - center);
  priority += max(0, 19 - center_dist);
  
  // Check for immediate threats/opportunities
  board[x][y] = player; // Temporarily place the move
  int my_score = calc_score_at(board, BOARD_SIZE, player, x, y);
  board[x][y] = other_player(player); // Check opponent's response
  int opp_score = calc_score_at(board, BOARD_SIZE, other_player(player), x, y);
  board[x][y] = AI_CELL_EMPTY; // Restore empty
  
  // Prioritize offensive and defensive moves
  priority += my_score / 10;   // Our opportunities
  priority += opp_score / 5;   // Blocking opponent
  
  return priority;
}

/**
 * Comparison function for sorting moves by priority (descending)
 */
int compare_moves(const void *a, const void *b) {
  move_t *move_a = (move_t *)a;
  move_t *move_b = (move_t *)b;
  return move_b->priority - move_a->priority; // Higher priority first
}

// Wrapper function for backward compatibility
int minimax(int **board, int depth, int alpha, int beta, int maximizing_player,
            int ai_player) {
  // Use center position as default for initial call
  int center = BOARD_SIZE / 2;
  return minimax_with_last_move(board, depth, alpha, beta, maximizing_player, ai_player, center, center);
}

int minimax_with_last_move(int **board, int depth, int alpha, int beta, int maximizing_player,
                          int ai_player, int last_x, int last_y) {

  // Check for immediate wins/losses first (terminal conditions)
  if (has_winner(board, BOARD_SIZE, ai_player)) {
    return WIN_SCORE + depth; // Prefer faster wins
  }
  if (has_winner(board, BOARD_SIZE, other_player(ai_player))) {
    return -WIN_SCORE - depth; // Prefer slower losses
  }
  
  // Check search depth limit
  if (depth == 0) {
    return evaluate_position_incremental(board, BOARD_SIZE, ai_player, last_x, last_y);
  }

  // Count stones once for this level
  int stones_on_board = 0;
  int moves_available = 0;
  for (int i = 0; i < BOARD_SIZE; i++) {
    for (int j = 0; j < BOARD_SIZE; j++) {
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

  int current_player_turn =
      maximizing_player ? ai_player : other_player(ai_player);

  // Generate and sort moves for better alpha-beta pruning
  move_t moves[BOARD_SIZE * BOARD_SIZE];
  int move_count = 0;
  
  // Collect all interesting moves
  for (int i = 0; i < BOARD_SIZE; i++) {
    for (int j = 0; j < BOARD_SIZE; j++) {
      if (board[i][j] == AI_CELL_EMPTY && is_move_interesting(board, i, j, stones_on_board)) {
        moves[move_count].x = i;
        moves[move_count].y = j;
        moves[move_count].priority = get_move_priority(board, i, j, current_player_turn);
        move_count++;
      }
    }
  }
  
  // Sort moves by priority (best first)
  qsort(moves, move_count, sizeof(move_t), compare_moves);

  if (maximizing_player) {
    int max_eval = -WIN_SCORE - 1;

    for (int m = 0; m < move_count; m++) {
      int i = moves[m].x;
      int j = moves[m].y;
      
      board[i][j] = current_player_turn;
      int eval = minimax_with_last_move(board, depth - 1, alpha, beta, 0, ai_player, i, j);
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
      int i = moves[m].x;
      int j = moves[m].y;
      
      board[i][j] = current_player_turn;
      int eval = minimax_with_last_move(board, depth - 1, alpha, beta, 1, ai_player, i, j);
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

/**
 * Find AI's first move: place randomly 1-2 squares away from human's first move
 */
void find_first_ai_move(int *best_x, int *best_y) {
  // Find the human's first move
  int human_x = -1, human_y = -1;
  for (int i = 0; i < BOARD_SIZE && human_x == -1; i++) {
    for (int j = 0; j < BOARD_SIZE && human_x == -1; j++) {
      if (board[i][j] == AI_CELL_BLACK) {
        human_x = i;
        human_y = j;
      }
    }
  }
  
  if (human_x == -1) {
    // Fallback: place in center if no human move found
    *best_x = BOARD_SIZE / 2;
    *best_y = BOARD_SIZE / 2;
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
        if (new_x >= 0 && new_x < BOARD_SIZE && 
            new_y >= 0 && new_y < BOARD_SIZE &&
            board[new_x][new_y] == AI_CELL_EMPTY) {
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
    printf("%s%s%s AI placed first move randomly near human's move\n", COLOR_BLUE, UNICODE_WHITE, COLOR_RESET);
  } else {
    // Fallback: place adjacent to human move
    *best_x = human_x + (rand() % 3 - 1); // -1, 0, or 1
    *best_y = human_y + (rand() % 3 - 1);
    
    // Ensure bounds
    *best_x = max(0, min(BOARD_SIZE - 1, *best_x));
    *best_y = max(0, min(BOARD_SIZE - 1, *best_y));
    printf("%s%s%s AI placed first move adjacent to human's move\n", COLOR_BLUE, UNICODE_WHITE, COLOR_RESET);
  }
}

void find_best_ai_move(int *best_x, int *best_y) {
  // Count stones on board to detect first AI move
  int stone_count = 0;
  for (int i = 0; i < BOARD_SIZE; i++) {
    for (int j = 0; j < BOARD_SIZE; j++) {
      if (board[i][j] != AI_CELL_EMPTY) {
        stone_count++;
      }
    }
  }
  
  // If there's exactly 1 stone (human's first move), use simple random placement
  if (stone_count == 1) {
    find_first_ai_move(best_x, best_y);
    add_ai_history_entry(1); // Random placement, 1 "move" considered
    return;
  }
  
  // Regular minimax for subsequent moves
  int best_score = -WIN_SCORE - 1;
  *best_x = -1;
  *best_y = -1;

  // Clear previous AI status message and show thinking message
  strcpy(ai_status_message, "");
  printf("%s%s%s AI is thinking", COLOR_BLUE, UNICODE_WHITE, COLOR_RESET);
  fflush(stdout);

  // Count stones once for move generation
  int stones_on_board = 0;
  for (int i = 0; i < BOARD_SIZE; i++) {
    for (int j = 0; j < BOARD_SIZE; j++) {
      if (board[i][j] != AI_CELL_EMPTY) {
        stones_on_board++;
      }
    }
  }

  // Check for immediate winning moves first
  for (int i = 0; i < BOARD_SIZE; i++) {
    for (int j = 0; j < BOARD_SIZE; j++) {
      if (board[i][j] == AI_CELL_EMPTY && is_move_interesting(board, i, j, stones_on_board)) {
        if (is_winning_move(board, i, j, AI_CELL_WHITE)) {
          *best_x = i;
          *best_y = j;
          snprintf(ai_status_message, sizeof(ai_status_message), 
                   "%s%s%s Found winning move immediately!", 
                   COLOR_BLUE, UNICODE_WHITE, COLOR_RESET);
          add_ai_history_entry(1); // Only checked 1 move
          return;
        }
      }
    }
  }

  // Generate and sort moves by priority
  move_t moves[BOARD_SIZE * BOARD_SIZE];
  int move_count = 0;
  
  for (int i = 0; i < BOARD_SIZE; i++) {
    for (int j = 0; j < BOARD_SIZE; j++) {
      if (board[i][j] == AI_CELL_EMPTY && is_move_interesting(board, i, j, stones_on_board)) {
        moves[move_count].x = i;
        moves[move_count].y = j;
        moves[move_count].priority = get_move_priority(board, i, j, AI_CELL_WHITE);
        move_count++;
      }
    }
  }
  
  // Sort moves by priority (best first)
  qsort(moves, move_count, sizeof(move_t), compare_moves);

  int moves_considered = 0;
  for (int m = 0; m < move_count; m++) {
    int i = moves[m].x;
    int j = moves[m].y;
    
    board[i][j] = AI_CELL_WHITE;
    int score = minimax_with_last_move(board, max_depth - 1, -WIN_SCORE - 1, WIN_SCORE + 1,
                                      0, AI_CELL_WHITE, i, j);
    board[i][j] = AI_CELL_EMPTY;

    if (score > best_score) {
      best_score = score;
      *best_x = i;
      *best_y = j;
      
      // Early termination for very good moves
      if (score >= WIN_SCORE - 1000) {
        snprintf(ai_status_message, sizeof(ai_status_message), 
                 "%s%s%s Found excellent move early! (evaluated %d moves)", 
                 COLOR_BLUE, UNICODE_WHITE, COLOR_RESET, moves_considered + 1);
        add_ai_history_entry(moves_considered + 1);
        return; // Exit function early to avoid duplicate history entry
      }
    }

    moves_considered++;
    printf("%s.%s", COLOR_BLUE, COLOR_RESET);
    fflush(stdout);
  }
  
  // Store the completion message if not already set by early termination
  if (strlen(ai_status_message) == 0) {
    snprintf(ai_status_message, sizeof(ai_status_message), 
             "%sDone! (evaluated %d moves)%s", 
             COLOR_BLUE, moves_considered, COLOR_RESET);
  }
  
  // Add to AI history
  add_ai_history_entry(moves_considered);
}

//===============================================================================
// GAME LOGIC
//===============================================================================

void check_game_state() {
  if (has_winner(board, BOARD_SIZE, AI_CELL_BLACK)) {
    game_state = GAME_HUMAN_WIN;
  } else if (has_winner(board, BOARD_SIZE, AI_CELL_WHITE)) {
    game_state = GAME_AI_WIN;
  } else {
    // Check for draw (board full)
    int empty_cells = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
      for (int j = 0; j < BOARD_SIZE; j++) {
        if (board[i][j] == AI_CELL_EMPTY) {
          empty_cells++;
        }
      }
    }
    if (empty_cells == 0) {
      game_state = GAME_DRAW;
    }
  }
}

void make_move(int x, int y, int player) {
  if (is_valid_move(x, y)) {
    // Record move in history before placing it
    add_move_to_history(x, y, player);
    
    board[x][y] = player;
    check_game_state();
    if (game_state == GAME_RUNNING) {
      current_player = other_player(current_player);
    }
  }
}

void handle_input() {
    int key = get_key();
    
    switch (key) {
        case KEY_UP:
            if (cursor_x > 0) cursor_x--;
            break;
        case KEY_DOWN:
            if (cursor_x < BOARD_SIZE - 1) cursor_x++;
            break;
        case KEY_LEFT:
            if (cursor_y > 0) cursor_y--;
            break;
        case KEY_RIGHT:
            if (cursor_y < BOARD_SIZE - 1) cursor_y++;
            break;
        case KEY_SPACE:
        case KEY_ENTER:
            if (current_player == AI_CELL_BLACK && is_valid_move(cursor_x, cursor_y)) {
                make_move(cursor_x, cursor_y, AI_CELL_BLACK);
            }
            break;
        case KEY_CTRL_Z:
            if (can_undo()) {
                undo_last_moves();
            }
            break;
        case '?':
            display_rules();
            break;
        case KEY_ESC:
        case 'q':
        case 'Q':
            game_state = GAME_QUIT;
            break;
        default:
            // Ignore other keys
            break;
    }
}

void print_usage(const char* program_name) {
  printf("Usage: %s [difficulty]\n", program_name);
  printf("\nDifficulty levels:\n");
  printf("  1 - Easy   (AI depth: 2, fast)\n");
  printf("  2 - Medium (AI depth: 4, default)\n");
  printf("  3 - Hard   (AI depth: 7, slow)\n");
  printf("\nGame symbols:\n");
  printf("  %s%s%s - Human player (red)\n", COLOR_RED, UNICODE_BLACK, COLOR_RESET);
  printf("  %s%s%s - AI player (blue)\n", COLOR_BLUE, UNICODE_WHITE, COLOR_RESET);
  printf("  %s%s%s - Current cursor (yellow)\n", COLOR_YELLOW, UNICODE_CURSOR, COLOR_RESET);
  printf("\nControls in game:\n");
  printf("  Arrow Keys - Move cursor\n");
  printf("  Space/Enter - Place stone\n");
  printf("  Ctrl-Z - Undo last move pair\n");
  printf("  ? - Show detailed game rules\n");
  printf("  ESC - Quit game\n");
  printf("\nExample: %s 1\n", program_name);
  printf("         %s 3\n", program_name);
}

int main(int argc, char* argv[]) {
  // Initialize random seed for first move randomization
  srand(time(NULL));
  
  // Parse command-line arguments
  if (argc > 1) {
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
      print_usage(argv[0]);
      return 0;
    }
    
    int difficulty = atoi(argv[1]);
    switch (difficulty) {
      case 1:
        max_depth = 3;
        break;
      case 2:
        max_depth = 6;
        break;
      case 3:
        max_depth = 9;
        break;
      default:
        printf("Error: Invalid difficulty level '%s'\n", argv[1]);
        printf("Valid options are 1 (easy), 2 (medium), or 3 (hard)\n\n");
        print_usage(argv[0]);
        return 1;
    }
  }
  
  board = create_board();
  enable_raw_mode();

  while (game_state == GAME_RUNNING) {
    clear_screen();
    draw_board();
    draw_status();
    
    if (current_player == AI_CELL_BLACK) {
      // Human's turn - wait for input
      handle_input();
    } else {
      // AI's turn - make move automatically
      int ai_x, ai_y;
      
      // Count stones to see if this is first AI move
      int stone_count = 0;
      for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
          if (board[i][j] != AI_CELL_EMPTY) {
            stone_count++;
          }
        }
      }
      
      find_best_ai_move(&ai_x, &ai_y);
      if (ai_x >= 0 && ai_y >= 0) {
        make_move(ai_x, ai_y, AI_CELL_WHITE);
      }
      
      // Shorter delay for first move, normal delay for complex moves
      if (stone_count == 1) {
        usleep(200000); // 0.2 seconds for first move
      } else {
        usleep(500000); // 0.5 seconds for regular moves
      }
    }
  }

  // Game ended - wait for final input
  if (game_state != GAME_QUIT) {
    clear_screen();
    draw_board();
    draw_status();
    get_key(); // Wait for any key press
  }

  free_board(board);
  return 0;
}