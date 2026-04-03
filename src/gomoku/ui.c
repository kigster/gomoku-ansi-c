//
//  ui.c
//  gomoku - User Interface module for display and input handling
//
//  Handles screen rendering, keyboard input, and user interactions
//

#include "ui.h"
#include "ansi.h"
#include "board.h"
#include "gomoku.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//===============================================================================
// INPUT HANDLING
//===============================================================================

struct termios original_termios;

int SHOW_LAST_MOVES = 35;

void disable_raw_mode(void) {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
}

void enable_raw_mode(void) {
  tcgetattr(STDIN_FILENO, &original_termios);
  atexit(disable_raw_mode);

  struct termios raw = original_termios;
  raw.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int get_key(void) {
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

void handle_input(game_state_t *game) {
  int key = get_key();

  switch (key) {
  case KEY_UP:
    if (game->cursor_x > 0)
      game->cursor_x--;
    break;
  case KEY_DOWN:
    if (game->cursor_x < game->board_size - 1)
      game->cursor_x++;
    break;
  case KEY_LEFT:
    if (game->cursor_y > 0)
      game->cursor_y--;
    break;
  case KEY_RIGHT:
    if (game->cursor_y < game->board_size - 1)
      game->cursor_y++;
    break;
  case KEY_SPACE:
  case KEY_ENTER:
    // Allow move if it's valid, regardless of which player
    if (is_valid_move(game->board, game->cursor_x, game->cursor_y,
                      game->board_size)) {
      double move_time = end_move_timer(game);
      make_move(game, game->cursor_x, game->cursor_y, game->current_player,
                move_time, 0, 0, 0);
    }
    break;
  case 'U':
  case 'u':
    if (can_undo(game)) {
      undo_last_moves(game);
    }
    break;
  case '?':
    display_rules();
    break;
  case KEY_ESC:
  case 'q':
  case 'Q':
    game->game_state = GAME_QUIT;
    break;
  default:
    // Ignore other keys
    break;
  }
}

//===============================================================================
// HINT MODE — THREAT PATTERN DETECTION
//===============================================================================

#define HINT_NONE   0
#define HINT_THREAT 1
#define MAX_BOARD_DIM 19

// Scan the board for threatening patterns and mark cells in hint_map.
// Highlighted patterns:
//   - Four in a row (open or closed on one side)
//   - Compound: two open threes for the same player
//   - Compound: open three + four for the same player
static void compute_hint_map(int **board, int board_size,
                             int hint_map[MAX_BOARD_DIM][MAX_BOARD_DIM]) {
  int dirs[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};

  // Collected patterns per player: fours and open threes
  // Store cell coordinates so we can mark them later
  typedef struct {
    int cells[5][2];
    int length;
    int open_ends;
    int player;
  } pattern_t;

  #define MAX_PATTERNS 200
  pattern_t fours[MAX_PATTERNS];
  int four_count = 0;
  pattern_t open_threes[MAX_PATTERNS];
  int open_three_count = 0;

  for (int d = 0; d < 4; d++) {
    int dx = dirs[d][0], dy = dirs[d][1];

    for (int i = 0; i < board_size; i++) {
      for (int j = 0; j < board_size; j++) {
        int player = board[i][j];
        if (player == AI_CELL_EMPTY)
          continue;

        // Only process the start of a run (previous cell is different)
        int pi = i - dx, pj = j - dy;
        if (pi >= 0 && pi < board_size && pj >= 0 && pj < board_size &&
            board[pi][pj] == player)
          continue;

        // Walk the run and record cells
        int len = 0;
        int cells[10][2];
        int cx = i, cy = j;
        while (cx >= 0 && cx < board_size && cy >= 0 && cy < board_size &&
               board[cx][cy] == player && len < 10) {
          cells[len][0] = cx;
          cells[len][1] = cy;
          len++;
          cx += dx;
          cy += dy;
        }

        // Check openness of each end
        int before_open =
            (pi >= 0 && pi < board_size && pj >= 0 && pj < board_size &&
             board[pi][pj] == AI_CELL_EMPTY);
        int after_open =
            (cx >= 0 && cx < board_size && cy >= 0 && cy < board_size &&
             board[cx][cy] == AI_CELL_EMPTY);
        int open_ends = before_open + after_open;

        if (len == 4 && open_ends > 0 && four_count < MAX_PATTERNS) {
          fours[four_count].length = len;
          fours[four_count].open_ends = open_ends;
          fours[four_count].player = player;
          for (int k = 0; k < len; k++) {
            fours[four_count].cells[k][0] = cells[k][0];
            fours[four_count].cells[k][1] = cells[k][1];
          }
          four_count++;
        } else if (len == 3 && open_ends == 2 &&
                   open_three_count < MAX_PATTERNS) {
          open_threes[open_three_count].length = len;
          open_threes[open_three_count].open_ends = open_ends;
          open_threes[open_three_count].player = player;
          for (int k = 0; k < len; k++) {
            open_threes[open_three_count].cells[k][0] = cells[k][0];
            open_threes[open_three_count].cells[k][1] = cells[k][1];
          }
          open_three_count++;
        }
      }
    }
  }

  // Mark all fours (open or closed — always dangerous)
  for (int f = 0; f < four_count; f++) {
    for (int k = 0; k < fours[f].length; k++) {
      hint_map[fours[f].cells[k][0]][fours[f].cells[k][1]] = HINT_THREAT;
    }
  }

  // For each player, check compound threats involving open threes
  for (int player = -1; player <= 1; player += 2) {
    int p_open_threes = 0, p_fours = 0;
    for (int f = 0; f < four_count; f++)
      if (fours[f].player == player)
        p_fours++;
    for (int t = 0; t < open_three_count; t++)
      if (open_threes[t].player == player)
        p_open_threes++;

    // Three-three or three-four compound: mark the open threes
    if (p_open_threes >= 2 || (p_open_threes >= 1 && p_fours >= 1)) {
      for (int t = 0; t < open_three_count; t++) {
        if (open_threes[t].player == player) {
          for (int k = 0; k < open_threes[t].length; k++) {
            hint_map[open_threes[t].cells[k][0]]
                    [open_threes[t].cells[k][1]] = HINT_THREAT;
          }
        }
      }
    }
  }
  #undef MAX_PATTERNS
}

//===============================================================================
// VICTORY HIGHLIGHT — FIND THE WINNING FIVE
//===============================================================================

#define WIN_CELL 1

// Locate the winning 5-in-a-row for the given player and mark those cells.
static void find_winning_cells(int **board, int board_size, int player,
                               int win_map[MAX_BOARD_DIM][MAX_BOARD_DIM]) {
  int dirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

  for (int i = 0; i < board_size; i++) {
    for (int j = 0; j < board_size; j++) {
      if (board[i][j] != player)
        continue;

      for (int d = 0; d < 4; d++) {
        int dx = dirs[d][0], dy = dirs[d][1];
        int count = 1;

        // Positive direction
        int x = i + dx, y = j + dy;
        while (x >= 0 && x < board_size && y >= 0 && y < board_size &&
               board[x][y] == player) {
          count++;
          x += dx;
          y += dy;
        }
        // Negative direction
        x = i - dx;
        y = j - dy;
        while (x >= 0 && x < board_size && y >= 0 && y < board_size &&
               board[x][y] == player) {
          count++;
          x -= dx;
          y -= dy;
        }

        if (count != 5)
          continue;

        // Walk back to the start of the line (negative-most end)
        int sx = i, sy = j;
        while (sx - dx >= 0 && sx - dx < board_size && sy - dy >= 0 &&
               sy - dy < board_size && board[sx - dx][sy - dy] == player) {
          sx -= dx;
          sy -= dy;
        }
        // Mark the 5 cells
        for (int k = 0; k < 5; k++) {
          win_map[sx + k * dx][sy + k * dy] = WIN_CELL;
        }
        return; // Only one winning line to highlight
      }
    }
  }
}

//===============================================================================
// DISPLAY FUNCTIONS
//===============================================================================

void clear_screen(void) { printf("\033[2J\033[H"); }

void press_any_key_to_continue(const char *press_what) {
  printf("\n%s%s%s%s%s\n\n", COLOR_BRIGHT_WHITE, ESCAPE_CODE_BOLD,
         "       ❯ Press ", press_what, COLOR_RESET);

  fflush(stdout);
  get_key();
  clear_screen();
}

void draw_game_header(void) {
  char buffer[2048];
  memset(buffer, 0, sizeof(buffer));
  snprintf(buffer, sizeof(buffer), "%s", GAME_RULES_LONG);

  printf(" %s%s%s%s\n", COLOR_RESET, COLOR_BRIGHT_YELLOW, buffer, COLOR_RESET);
  press_any_key_to_continue("ENTER to continue...");

  printf("\n");
  printf(" %s%s %s(v%s%s)\n\n", COLOR_YELLOW, GAME_DESCRIPTION, COLOR_RED,
         GAME_VERSION, COLOR_RESET);
  printf(" %s%s%s\n\n", COLOR_BRIGHT_GREEN, GAME_COPYRIGHT, COLOR_RESET);
  printf("  %s%s%s\n", ESCAPE_CODE_BOLD, COLOR_BRIGHT_WHITE, "HOW TO PLAY:");
  printf(" %s%s%s\n\n", ESCAPE_CODE_BOLD, COLOR_BRIGHT_WHITE,
         "───────────────────────────────────────────────────────────────");
  printf(" %s%s%s\n\n\n", ESCAPE_CODE_BOLD, COLOR_MAGENTA, GAME_RULES_BRIEF);
  press_any_key_to_continue("ENTER to start the game...");
}

void draw_game_history_sidebar(game_state_t *game, int start_row) {
  // Position cursor to the right of the board
  int sidebar_col = game->board_size * 2 + 12; // After board and some spacing
  if (sidebar_col > 50) {
    sidebar_col = 50;
  } else if (sidebar_col < 50) {
    sidebar_col = 50;
  }

  // Draw Game History header
  printf(ESCAPE_MOVE_CURSOR_TO, start_row, sidebar_col);
  printf("%s%sGame History:%s", COLOR_BOLD_BLACK, COLOR_GREEN, COLOR_RESET);

  printf(ESCAPE_MOVE_CURSOR_TO, start_row + 1, sidebar_col);
  printf("%sMove  Time%s", COLOR_BOLD_BLACK, COLOR_RESET);

  printf(ESCAPE_MOVE_CURSOR_TO, start_row + 2, sidebar_col);
  printf("%s", "────────────────────────────────────");

  // Draw move history entries
  int display_start =
      max(0, game->move_history_count - SHOW_LAST_MOVES); // Show last 15 moves
  for (int i = display_start; i < game->move_history_count; i++) {
    move_history_t move = game->move_history[i];
    char player_symbol = (move.player == AI_CELL_CROSSES) ? 'X' : 'O';
    const char *player_color =
        (move.player == AI_CELL_CROSSES) ? COLOR_RED : COLOR_GREEN;

    char coord_buf[8];
    board_coord_to_notation(move.x, move.y, coord_buf, sizeof(coord_buf));

    // Determine if this was an AI or human move
    int player_index = (move.player == AI_CELL_CROSSES) ? 0 : 1;
    player_type_t move_player_type = game->player_type[player_index];

    char move_line[80];
    if (move_player_type == PLAYER_TYPE_HUMAN) {
      snprintf(move_line, sizeof(move_line), "%s%3d | %c: %3s, %6.3fs%s",
               player_color, i + 1, player_symbol, coord_buf, move.time_taken,
               COLOR_RESET);
    } else {
      double rate = (move.time_taken > 0.0 && move.positions_evaluated > 0)
                        ? (double)move.positions_evaluated / move.time_taken
                        : 0.0;
      snprintf(move_line, sizeof(move_line),
               "%s%3d | %c: %3s, %6.3fs @ %6.0f moves/second%s", player_color,
               i + 1, player_symbol, coord_buf, move.time_taken, rate,
               COLOR_RESET);
    }

    printf("\033[%d;%dH%s", start_row + 3 + (i - display_start), sidebar_col,
           move_line);
  }
}

void draw_board(game_state_t *game) {
  printf("\n     ");

  // Column letters with negative circled characters (A–T, skipping I)
  for (int j = 0; j < game->board_size; j++) {
    if (j > 9) {
      printf("%s%s%s ", COLOR_BLUE, get_column_letter_unicode(j), COLOR_RESET);
    } else {
      printf("%s%s%s ", COLOR_GREEN, get_column_letter_unicode(j), COLOR_RESET);
    }
  }
  printf("\n");

  int board_start_row =
      0; // After header (4 lines) + column numbers (1 line) + buffer (1 line)

  // Determine if current player is human (cursor only shown for human players,
  // not in replay mode)
  int current_player_index = (game->current_player == AI_CELL_CROSSES) ? 0 : 1;
  int is_human_turn =
      !game->replay_mode &&
      (game->player_type[current_player_index] == PLAYER_TYPE_HUMAN);

  // Find the last move position for highlighting (from move history)
  int last_move_x = -1, last_move_y = -1;
  if (game->move_history_count > 0) {
    last_move_x = game->move_history[game->move_history_count - 1].x;
    last_move_y = game->move_history[game->move_history_count - 1].y;
  }

  // Compute hint map if hints are enabled
  int hint_map[MAX_BOARD_DIM][MAX_BOARD_DIM];
  memset(hint_map, 0, sizeof(hint_map));
  if (game->config.hints_enabled) {
    compute_hint_map(game->board, game->board_size, hint_map);
  }

  // Compute winning cells map if someone won
  int win_map[MAX_BOARD_DIM][MAX_BOARD_DIM];
  memset(win_map, 0, sizeof(win_map));
  int winner_player = 0;
  if (game->game_state == GAME_HUMAN_WIN) {
    winner_player = AI_CELL_CROSSES;
    find_winning_cells(game->board, game->board_size, winner_player, win_map);
  } else if (game->game_state == GAME_AI_WIN) {
    winner_player = AI_CELL_NAUGHTS;
    find_winning_cells(game->board, game->board_size, winner_player, win_map);
  }

  for (int i = 0; i < game->board_size; i++) {
    printf("  ");
    if (i > 9) {
      printf("%s%2s%s ", COLOR_BLUE, get_coordinate_unicode(i - 10),
             COLOR_RESET);
    } else {
      printf("%s%2s%s ", COLOR_GREEN, get_coordinate_unicode(i), COLOR_RESET);
    }
    for (int j = 0; j < game->board_size; j++) {
      // Only show cursor if it's a human player's turn
      int is_cursor_here =
          is_human_turn && (i == game->cursor_x && j == game->cursor_y);
      int is_last_move = (i == last_move_x && j == last_move_y);

      printf(" "); // Always add the space before the symbol

      // Show appropriate symbol based on cell content
      if (game->board[i][j] == AI_CELL_EMPTY) {
        if (is_cursor_here) {
          // Empty cell with cursor: show yellow X or O based on current player
          if (game->current_player == AI_CELL_CROSSES) {
            printf("%s%s%s", COLOR_CURSOR, UNICODE_CROSSES, COLOR_RESET);
          } else {
            printf("%s%s%s", COLOR_CURSOR, UNICODE_NAUGHTS, COLOR_RESET);
          }
        } else {
          // Empty cell without cursor: show normal grid intersection
          printf("%s%s%s", COLOR_RESET, UNICODE_EMPTY, COLOR_RESET);
        }
      } else if (game->board[i][j] == AI_CELL_CROSSES) {
        if (is_cursor_here) {
          printf("%s%s%s%s", COLOR_X_NORMAL, COLOR_BG_CURSOR_OCCUPIED,
                 UNICODE_CROSSES, COLOR_RESET);
        } else if (win_map[i][j] == WIN_CELL) {
          printf("%s%s%s", COLOR_X_WIN, UNICODE_CROSSES, COLOR_RESET);
        } else if (is_last_move) {
          printf("%s%s%s", COLOR_X_LAST_MOVE, UNICODE_CROSSES, COLOR_RESET);
        } else if (hint_map[i][j] == HINT_THREAT) {
          printf("%s%s%s", COLOR_X_HINT, UNICODE_CROSSES, COLOR_RESET);
        } else {
          printf("%s%s%s", COLOR_X_NORMAL, UNICODE_CROSSES, COLOR_RESET);
        }
      } else { // AI_CELL_NAUGHTS
        if (is_cursor_here) {
          printf("%s%s%s%s", COLOR_O_NORMAL, COLOR_BG_CURSOR_OCCUPIED,
                 UNICODE_NAUGHTS, COLOR_RESET);
        } else if (win_map[i][j] == WIN_CELL) {
          printf("%s%s%s", COLOR_O_WIN, UNICODE_NAUGHTS, COLOR_RESET);
        } else if (is_last_move) {
          printf("%s%s%s", COLOR_O_LAST_MOVE, UNICODE_NAUGHTS, COLOR_RESET);
        } else if (hint_map[i][j] == HINT_THREAT) {
          printf("%s%s%s", COLOR_O_HINT, UNICODE_NAUGHTS, COLOR_RESET);
        } else {
          printf("%s%s%s", COLOR_O_NORMAL, UNICODE_NAUGHTS, COLOR_RESET);
        }
      }
    }
    printf("\n");
  }

  // Draw game history sidebar
  draw_game_history_sidebar(game, board_start_row + 2);
}

void draw_status(game_state_t *game) {
  // Add spacing and lock the box to a position
  printf(ESCAPE_MOVE_CURSOR_TO, 24, 1);

  char *prefix;
  prefix = malloc(100);
  memset(prefix, 0, 100);
  sprintf(prefix, "%s%s", ESCAPE_CODE_RESET, "  ");

  // Box width for the status border
  const int box_width = 19 * 2 + 2;
  const int control_width = 14;
  const int action_width = box_width - control_width - 6;

  // Top border
  printf("%s%s┌", prefix, COLOR_RESET);
  for (int i = 0; i < box_width - 2; i++) {
    printf("─");
  }
  printf("┐%s\n", COLOR_RESET);

  // Current Player
  int current_player_index = (game->current_player == AI_CELL_CROSSES) ? 0 : 1;
  player_type_t current_type = game->player_type[current_player_index];
  char player_symbol = (game->current_player == AI_CELL_CROSSES) ? 'X' : 'O';
  const char *player_type_str =
      (current_type == PLAYER_TYPE_HUMAN) ? "Human" : "Computer";

  if (game->current_player == AI_CELL_CROSSES) {
    char current_player_str[64];
    snprintf(current_player_str, sizeof(current_player_str),
             "Current Player : %s (%c)", player_type_str, player_symbol);
    printf("%s│%s %-*s %s│%s%s\n", prefix, COLOR_YELLOW,
           action_width + control_width + 2, current_player_str, COLOR_RESET,
           COLOR_RESET, COLOR_YELLOW);
  } else {
    char current_player_str[64];
    snprintf(current_player_str, sizeof(current_player_str),
             "Current Player : %s (%c)", player_type_str, player_symbol);
    printf("%s│%s %-*s %s│%s%s\n", prefix, COLOR_BLUE,
           action_width + control_width + 2, current_player_str, COLOR_RESET,
           COLOR_RESET, COLOR_BLUE);
  }

  // Position (convert to 1-based coordinates for display)
  char position_str[32];
  snprintf(position_str, sizeof(position_str), "Position       : [ %2d, %2d ]",
           board_to_display_coord(game->cursor_x),
           board_to_display_coord(game->cursor_y));

  printf("%s%s│ %-*s │\n", prefix, COLOR_RESET, box_width - 4, position_str);

  // Difficulty
  const char *difficulty_name;
  const char *difficulty_color;
  switch (game->max_depth) {
  case GAME_DEPTH_LEVEL_EASY:
    difficulty_name = "Easy";
    difficulty_color = COLOR_GREEN;
    break;
  case GAME_DEPTH_LEVEL_MEDIUM:
    difficulty_name = "Intermediate";
    difficulty_color = COLOR_YELLOW;
    break;
  case GAME_DEPTH_LEVEL_HARD:
    difficulty_name = "Hard";
    difficulty_color = COLOR_RED;
    break;
  default:
    difficulty_name = "Custom";
    difficulty_color = COLOR_MAGENTA;
  }

  // Difficulty
  // Provide extra space for color codes and formatting to avoid truncation
  // warnings
  char difficulty_str[box_width + 16];
  char difficulty_val[box_width + 16];
  memset(difficulty_str, 0, sizeof(difficulty_str));
  memset(difficulty_val, 0, sizeof(difficulty_val));

  snprintf(difficulty_str, sizeof(difficulty_str), "%sDifficulty     : %s",
           difficulty_color, difficulty_name);
  printf("%s%s│ %s", prefix, COLOR_RESET, difficulty_str);
  printf(ESCAPE_MOVE_CURSOR_TO, 27, 42);
  printf("%s│%s\n", COLOR_RESET, COLOR_RESET);

  // Search Depth - show both depths if different
  memset(difficulty_str, 0, sizeof(difficulty_str));
  if (game->depth_for_player[0] != game->depth_for_player[1]) {
    snprintf(difficulty_str, sizeof(difficulty_str),
             "%sSearch Depth   : X=%d, O=%d", COLOR_MAGENTA,
             game->depth_for_player[0], game->depth_for_player[1]);
  } else {
    snprintf(difficulty_str, sizeof(difficulty_str), "%sSearch Depth   : %d",
             difficulty_color, game->max_depth);
  }
  printf("%s%s│ %s", prefix, COLOR_RESET, difficulty_str);
  printf(ESCAPE_MOVE_CURSOR_TO, 28, 42);
  printf("%s│%s\n", COLOR_RESET, COLOR_RESET);

  // Search Radius
  memset(difficulty_str, 0, sizeof(difficulty_str));
  snprintf(difficulty_str, sizeof(difficulty_str), "%sSearch Radius  : %d",
           difficulty_color, game->search_radius);
  printf("%s%s│ %s", prefix, COLOR_RESET, difficulty_str);
  printf(ESCAPE_MOVE_CURSOR_TO, 29, 42);
  printf("%s│%s\n", COLOR_RESET, COLOR_RESET);

  // Separator line
  printf("%s%s│ %-*s %s│%s\n", prefix, COLOR_RESET, box_width - 4, "",
         COLOR_RESET, COLOR_RESET);

  // Controls header
  printf("%s%s│ %s%-*s %s│\n", prefix, COLOR_RESET, COLOR_BRIGHT_BLUE,
         box_width - 4, "Controls", COLOR_RESET);

  // Control instructions
  printf("%s%s│ %s%-*s — %s%-*s%s│\n", prefix, COLOR_RESET, COLOR_BRIGHT_YELLOW,
         control_width, "Arrow Keys", COLOR_GREEN, action_width, "Move cursor",
         COLOR_RESET);
  printf("%s%s│ %s%-*s — %s%-*s%s│\n", prefix, COLOR_RESET, COLOR_BRIGHT_YELLOW,
         control_width, "Space / Enter", COLOR_GREEN, action_width, "Make move",
         COLOR_RESET);
  if (game->config.enable_undo) {
    printf("%s%s│ %s%-*s — %s%-*s%s│\n", prefix, COLOR_RESET,
           COLOR_BRIGHT_YELLOW, control_width, "U", COLOR_GREEN, action_width,
           "Undo last move pair", COLOR_RESET);
  }
  printf("%s%s│ %s%-*s — %s%-*s%s│\n", prefix, COLOR_RESET, COLOR_BRIGHT_YELLOW,
         control_width, "?", COLOR_GREEN, action_width, "Show game rules",
         COLOR_RESET);
  printf("%s%s│ %s%-*s — %s%-*s%s│\n", prefix, COLOR_RESET, COLOR_BRIGHT_YELLOW,
         control_width, "ESC", COLOR_GREEN, action_width, "Quit game",
         COLOR_RESET);

  printf("%s%s│ %-*s │%s\n", prefix, COLOR_RESET, box_width - 4, " ",
         COLOR_RESET);

  // AI status message if available
  if (strlen(game->ai_status_message) > 0) {
    printf("%s%s├%-*s┤%s\n", prefix, COLOR_RESET, box_width - 4,
           "──────────────────────────────────────", COLOR_RESET);

    // Extract clean message without ANSI color codes
    char clean_message[100] = "";
    const char *msg = game->ai_status_message;
    char *clean_ptr = clean_message;

    while (*msg &&
           (clean_ptr - clean_message) < (int)(sizeof(clean_message) - 1)) {
      if (*msg == '\033') {
        // Skip ANSI escape sequence
        while (*msg && *msg != 'm')
          msg++;
        if (*msg)
          msg++;
      } else {
        *clean_ptr++ = *msg++;
      }
    }
    *clean_ptr = '\0';

    printf("%s%s│%s %-*s %s│\n", prefix, COLOR_RESET, COLOR_MAGENTA,
           box_width - 4, clean_message, COLOR_RESET);
  }

  // Game state messages
  if (game->game_state != GAME_RUNNING) {
    printf("%s%s├%-*s┤%s\n", prefix, COLOR_RESET, box_width - 4,
           "──────────────────────────────────────", COLOR_RESET);

    switch (game->game_state) {
    case GAME_HUMAN_WIN: {
      // CROSSES (X) won
      int winner_index = 0; // CROSSES
      int loser_index = 1;  // NAUGHTS
      player_type_t winner_type = game->player_type[winner_index];
      player_type_t loser_type = game->player_type[loser_index];
      int winner_depth = game->depth_for_player[winner_index];
      int loser_depth = game->depth_for_player[loser_index];

      char winner_str[80], loser_str[80];
      if (winner_type == PLAYER_TYPE_HUMAN) {
        snprintf(winner_str, sizeof(winner_str), "Winner: X (Human)");
      } else {
        const char *level_name = (winner_depth <= 2)   ? "Easy"
                                 : (winner_depth <= 4) ? "Medium"
                                                       : "Hard";
        snprintf(winner_str, sizeof(winner_str), "Winner: X (AI @ %s)",
                 level_name);
      }

      if (loser_type == PLAYER_TYPE_HUMAN) {
        snprintf(loser_str, sizeof(loser_str), "Loser : O (Human)");
      } else {
        const char *level_name = (loser_depth <= 2)   ? "Easy"
                                 : (loser_depth <= 4) ? "Medium"
                                                      : "Hard";
        snprintf(loser_str, sizeof(loser_str), "Loser : O (AI @ %s)",
                 level_name);
      }

      printf("%s%s│ %-*s %s│\n", prefix, COLOR_RESET, box_width - 4, winner_str,
             COLOR_RESET);
      printf("%s%s│ %-*s %s│\n", prefix, COLOR_RESET, box_width - 4, loser_str,
             COLOR_RESET);
      break;
    }
    case GAME_AI_WIN: {
      // NAUGHTS (O) won
      int winner_index = 1; // NAUGHTS
      int loser_index = 0;  // CROSSES
      player_type_t winner_type = game->player_type[winner_index];
      player_type_t loser_type = game->player_type[loser_index];
      int winner_depth = game->depth_for_player[winner_index];
      int loser_depth = game->depth_for_player[loser_index];

      char winner_str[80], loser_str[80];
      if (winner_type == PLAYER_TYPE_HUMAN) {
        snprintf(winner_str, sizeof(winner_str), "Winner: O (Human)");
      } else {
        const char *level_name = (winner_depth <= 2)   ? "Easy"
                                 : (winner_depth <= 4) ? "Medium"
                                                       : "Hard";
        snprintf(winner_str, sizeof(winner_str), "Winner: O (AI @ %s)",
                 level_name);
      }

      if (loser_type == PLAYER_TYPE_HUMAN) {
        snprintf(loser_str, sizeof(loser_str), "Loser : X (Human)");
      } else {
        const char *level_name = (loser_depth <= 2)   ? "Easy"
                                 : (loser_depth <= 4) ? "Medium"
                                                      : "Hard";
        snprintf(loser_str, sizeof(loser_str), "Loser : X (AI @ %s)",
                 level_name);
      }

      printf("%s%s│ %-*s %s│\n", prefix, COLOR_RESET, box_width - 4, winner_str,
             COLOR_RESET);
      printf("%s%s│ %-*s %s│\n", prefix, COLOR_RESET, box_width - 4, loser_str,
             COLOR_RESET);
      break;
    }
    case GAME_DRAW:
      printf("%s%s│%s %-*s %s", prefix, COLOR_RESET, COLOR_RESET, control_width,
             "The Game is a draw!", COLOR_RESET);
      printf(ESCAPE_MOVE_CURSOR_TO, 40, 42);
      printf("%s│%s\n", COLOR_RESET, COLOR_RESET);
      printf("%s%s│ %-*s │%s\n", prefix, COLOR_RESET, box_width - 4, " ",
             COLOR_RESET);
      break;
    }

    // Show timing summary
    char time_summary[100];
    const char *x_label =
        (game->player_type[0] == PLAYER_TYPE_HUMAN) ? "Human(X)" : "AI(X)";
    const char *o_label =
        (game->player_type[1] == PLAYER_TYPE_HUMAN) ? "Human(O)" : "AI(O)";
    snprintf(time_summary, sizeof(time_summary),
             "%s%s: %.1fs %s|%s %s: %.1fs%s", COLOR_BRIGHT_BLUE, x_label,
             game->total_human_time, // CROSSES time
             COLOR_RESET, COLOR_BRIGHT_CYAN, o_label, game->total_ai_time,
             COLOR_RESET); // NAUGHTS time
    printf("%s%s│ %-*s %s", prefix, COLOR_RESET, box_width - 4, time_summary,
           COLOR_RESET);

    printf(ESCAPE_MOVE_CURSOR_TO, 42, 42);
    printf("%s│%s\n", COLOR_RESET, COLOR_RESET);

    printf("%s%s│ %-*s %s│\n", prefix, COLOR_YELLOW, box_width - 4,
           "Press any key to exit...", COLOR_RESET);
  }

  // Bottom border
  printf("  %s└", COLOR_RESET);
  for (int i = 0; i < box_width - 2; i++) {
    printf("─");
  }
  printf("┘%s\n", COLOR_RESET);
  free(prefix);
}

void display_rules(void) {
  clear_screen();

  printf("%s═══════════════════════════════════════════════════════════════════"
         "════════════%s\n",
         COLOR_RESET, COLOR_RESET);
  printf("%s        GOMOKU RULES & HELP (RECOMMENDED TO HAVE 66-LINE TERMINAL) "
         "            %s\n",
         COLOR_RESET, COLOR_RESET);
  printf("%s═══════════════════════════════════════════════════════════════════"
         "════════════%s\n",
         COLOR_RESET, COLOR_RESET);
  printf("\n");

  // Basic objective
  printf("%sOBJECTIVE%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("   Gomoku (Five in a Row) is a strategy game where players take "
         "turns placing\n");
  printf("   stones on a board. The goal is to be the first to get five stones "
         "in a row\n");
  printf("   (horizontally, vertically, or diagonally).\n\n");

  // Game pieces
  printf("%sGAME PIECES%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("   %s%s%s          — Human Player (Crosses) - You play first\n",
         COLOR_RED, UNICODE_CROSSES, COLOR_RESET);
  printf("   %s%s%s          — AI Player (Naughts) - Computer opponent\n",
         COLOR_BLUE, UNICODE_NAUGHTS, COLOR_RESET);
  printf("   %s%s%s          — Cursor (yellow, matches your piece)\n",
         COLOR_CURSOR, UNICODE_CROSSES, COLOR_RESET);
  printf("   %s%s%s%s          — Cursor on occupied cell\n\n", COLOR_X_NORMAL,
         COLOR_BG_CURSOR_OCCUPIED, UNICODE_CROSSES, COLOR_RESET);

  // How to play
  printf("%sHOW TO PLAY%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("   1. Crosses (Human) always goes first\n");
  printf("   2. Players alternate turns placing one stone per turn\n");
  printf("   3. Stones are placed on intersections of the grid lines\n");
  printf("   4. Once placed, stones cannot be moved or removed\n");
  printf("   5. Win by creating an unbroken line of exactly 5 stones\n\n");

  // Winning conditions
  printf("%sWINNING CONDITIONS%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("   Win by creating an unbroken line of exactly 5 stones:\n");
  printf("   • Horizontal: %s%s%s %s%s%s %s%s%s %s%s%s %s%s%s\n", COLOR_RED,
         UNICODE_CROSSES, COLOR_RESET, COLOR_RED, UNICODE_CROSSES, COLOR_RESET,
         COLOR_RED, UNICODE_CROSSES, COLOR_RESET, COLOR_RED, UNICODE_CROSSES,
         COLOR_RESET, COLOR_RED, UNICODE_CROSSES, COLOR_RESET);
  printf("   • Vertical:   Lines going up and down\n");
  printf("   • Diagonal:   Lines going diagonally in any direction\n");
  printf("   • Six or more stones in a row do NOT count as a win (overline "
         "rule)\n\n");

  // Basic strategies
  printf("%sBASIC STRATEGIES%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("   %sOffense & Defense:%s Balance creating your own lines with "
         "blocking\n",
         COLOR_BOLD_BLACK, COLOR_RESET);
  printf("   opponent's attempts to get five in a row.\n\n");

  printf("   %sControl the Center:%s The center of the board provides more\n",
         COLOR_BOLD_BLACK, COLOR_RESET);
  printf("   opportunities to create lines in multiple directions.\n\n");

  printf("   %sWatch for Threats:%s An 'open three' (three stones with both "
         "ends\n",
         COLOR_BOLD_BLACK, COLOR_RESET);
  printf("   open) must be blocked immediately, or it becomes an unstoppable "
         "'open four'.\n\n");

  // Controls reminder
  printf("%sGAME CONTROLS%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("   • Arrow Keys: Move cursor\n");
  printf("   • Space/Enter: Place stone\n");
  printf("   • U: Undo last move pair (human + AI) if enabled\n");
  printf("   • ?: Show this help screen\n");
  printf("   • ESC: Quit game\n\n");

  // CLI options
  printf("%sCOMMAND LINE OPTIONS%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf(
      "   -d, --depth N        Search depth (1-10) for AI minimax algorithm\n");
  printf("   -l, --level M        Difficulty: easy, medium, hard\n");
  printf("   -t, --timeout T      Move timeout in seconds (optional)\n");
  printf("   -b, --board SIZE     Board size: 15 or 19 (default: 19)\n");
  printf("   -h, --help           Show command line help\n\n");

  printf("%sEXAMPLES%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("   gomoku --level easy --board 15\n");
  printf("   gomoku -d 4 -t 30 -b 19\n");
  printf("   gomoku --level hard --timeout 60\n\n");

  // Game variations
  printf("%sDIFFICULTY LEVELS%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
  printf("   • Easy (depth %d):         Quick moves, good for beginners\n",
         GAME_DEPTH_LEVEL_EASY);
  printf("   • Medium (depth %d):       Balanced gameplay, default setting\n",
         GAME_DEPTH_LEVEL_MEDIUM);
  printf(
      "   • Hard (depth %d):         Advanced AI, challenging for experts\n\n",
      GAME_DEPTH_LEVEL_HARD);

  printf("%s═══════════════════════════════════════════════════════════════════"
         "════════════%s\n",
         COLOR_BOLD_BLACK, COLOR_RESET);
  printf("                      %sPress any key to return to game%s\n",
         COLOR_YELLOW, COLOR_RESET);
  printf("%s═══════════════════════════════════════════════════════════════════"
         "════════════%s\n",
         COLOR_BOLD_BLACK, COLOR_RESET);

  // Wait for any key press
  get_key();
}

void refresh_display(game_state_t *game) {
  clear_screen();
  draw_board(game);
  draw_status(game);
}

void position_cursor_near_last_move(game_state_t *game) {
  // Position cursor on an empty cell near the last move
  // This is called after AI moves when the next player is human

  int last_x = -1, last_y = -1;

  // Find the last move position
  if (game->move_history_count > 0) {
    last_x = game->move_history[game->move_history_count - 1].x;
    last_y = game->move_history[game->move_history_count - 1].y;
  }

  // If no moves yet, position at center
  if (last_x < 0 || last_y < 0) {
    game->cursor_x = game->board_size / 2;
    game->cursor_y = game->board_size / 2;
    return;
  }

  // Search in expanding radius for an empty cell
  for (int radius = 1; radius <= 3; radius++) {
    for (int dx = -radius; dx <= radius; dx++) {
      for (int dy = -radius; dy <= radius; dy++) {
        // Only check cells at exactly this radius (perimeter)
        if (abs(dx) != radius && abs(dy) != radius) {
          continue;
        }

        int nx = last_x + dx;
        int ny = last_y + dy;

        // Check bounds
        if (nx < 0 || nx >= game->board_size || ny < 0 ||
            ny >= game->board_size) {
          continue;
        }

        // Check if empty
        if (game->board[nx][ny] == AI_CELL_EMPTY) {
          game->cursor_x = nx;
          game->cursor_y = ny;
          return;
        }
      }
    }
  }

  // Fallback: if no empty cell found nearby, search from last move position
  // outward
  for (int i = 0; i < game->board_size; i++) {
    for (int j = 0; j < game->board_size; j++) {
      if (game->board[i][j] == AI_CELL_EMPTY) {
        game->cursor_x = i;
        game->cursor_y = j;
        return;
      }
    }
  }
}
