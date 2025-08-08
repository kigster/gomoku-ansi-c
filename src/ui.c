//
//  ui.c
//  gomoku - User Interface module for display and input handling
//
//  Handles screen rendering, keyboard input, and user interactions
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ui.h"
#include "ansi.h"
#include "gomoku_c.h"

//===============================================================================
// INPUT HANDLING
//===============================================================================

struct termios original_termios;

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
            if (game->cursor_x > 0) game->cursor_x--;
            break;
        case KEY_DOWN:
            if (game->cursor_x < game->board_size - 1) game->cursor_x++;
            break;
        case KEY_LEFT:
            if (game->cursor_y > 0) game->cursor_y--;
            break;
        case KEY_RIGHT:
            if (game->cursor_y < game->board_size - 1) game->cursor_y++;
            break;
        case KEY_SPACE:
        case KEY_ENTER:
            if (game->current_player == AI_CELL_CROSSES && 
                    is_valid_move(game->board, game->cursor_x, game->cursor_y, game->board_size)) {
                double move_time = end_move_timer(game);
                make_move(game, game->cursor_x, game->cursor_y, AI_CELL_CROSSES, move_time, 0);
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
// DISPLAY FUNCTIONS
//===============================================================================

void clear_screen(void) {
    printf("\033[2J\033[H");
}

void draw_game_header(void) {
    printf("\n");
    printf(" %s%s %s(v%s%s)\n\n", COLOR_YELLOW, GAME_DESCRIPTION, COLOR_RED, GAME_VERSION, COLOR_RESET);
    printf(" %s%s%s\n\n", COLOR_BRIGHT_GREEN, GAME_COPYRIGHT, COLOR_RESET);
    printf(" %s%s%s\n", ESCAPE_CODE_BOLD, COLOR_MAGENTA, "HINT:");
    printf(" %s%s%s\n\n\n", ESCAPE_CODE_BOLD, COLOR_MAGENTA, GAME_RULES_BRIEF);
    printf(" %s%s%s%s\n\n\n", COLOR_RESET, COLOR_BRIGHT_CYAN, GAME_RULES_LONG, COLOR_RESET);
    printf("\n\n\n %s%s%s%s\n\n\n\n\n\n\n", COLOR_YELLOW, ESCAPE_CODE_BOLD, "Press ENTER to start the game, or CTRL-C to quit...", COLOR_RESET);

    fflush(stdout);
    get_key();
    clear_screen();
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
    printf("%sMove Player [Time] (AI positions evaluated)%s",
            COLOR_BOLD_BLACK, COLOR_RESET);

    printf(ESCAPE_MOVE_CURSOR_TO, start_row + 2, sidebar_col);
    printf("%s", "───────────────────────────────────────────────");

    // Draw move history entries
    int display_start = max(0, game->move_history_count - 15); // Show last 15 moves
    for (int i = display_start; i < game->move_history_count; i++) {
        move_history_t move = game->move_history[i];
        char player_symbol = (move.player == AI_CELL_CROSSES) ? 'x' : 'o';
        const char* player_color = (move.player == AI_CELL_CROSSES) ? COLOR_RED : COLOR_BLUE;

        char move_line[100];
        if (move.player == AI_CELL_CROSSES) {
            // Human move (convert to 1-based coordinates for display)
            snprintf(move_line, sizeof(move_line), 
                    "%s%2d | player %c moved to [%2d, %2d] (in %6.2fs)%s",
                    player_color, i + 1, player_symbol, 
                    board_to_display_coord(move.x), board_to_display_coord(move.y), 
                    move.time_taken, COLOR_RESET);
        } else {
            // AI move (convert to 1-based coordinates for display)
            snprintf(move_line, sizeof(move_line), 
                    "%s%2d | player %c moved to [%2d, %2d] (in %6.2fs, %3d moves evaluated)%s",
                    player_color, i + 1, player_symbol, 
                    board_to_display_coord(move.x), board_to_display_coord(move.y), 
                    move.time_taken, move.positions_evaluated, COLOR_RESET);
        }

        printf("\033[%d;%dH%s", start_row + 3 + (i - display_start), sidebar_col, move_line);
    }
}

void draw_board(game_state_t *game) {
    printf("\n     ");

    // Column numbers with Unicode characters
    for (int j = 0; j < game->board_size; j++) {
        if (j > 9) {    
            printf("%s%2s%s ", COLOR_BLUE, get_coordinate_unicode(j - 10), COLOR_RESET);
        } else {
            printf("%s%2s%s ", COLOR_GREEN, get_coordinate_unicode(j), COLOR_RESET);
        }
    }
    printf("\n");

    int board_start_row = 0; // After header (4 lines) + column numbers (1 line) + buffer (1 line)

    for (int i = 0; i < game->board_size; i++) {
        printf("  ");
        if (i > 9) {    
            printf("%s%2s%s ", COLOR_BLUE, get_coordinate_unicode(i - 10), COLOR_RESET);
        } else {
            printf("%s%2s%s ", COLOR_GREEN, get_coordinate_unicode(i), COLOR_RESET);
        }
        for (int j = 0; j < game->board_size; j++) {
            // Check if cursor is at this position
            int is_cursor_here = (i == game->cursor_x && j == game->cursor_y);

            printf(" "); // Always add the space before the symbol

            // Show appropriate symbol based on cell content
            if (game->board[i][j] == AI_CELL_EMPTY) {
                if (is_cursor_here) {
                    // Empty cell with cursor: show yellow blinking cursor (no background)
                    printf("%s%s%s", COLOR_X_CURSOR, UNICODE_CURSOR, COLOR_RESET);
                } else {
                    // Empty cell without cursor: show normal grid intersection
                    printf("%s%s%s", COLOR_RESET, UNICODE_EMPTY, COLOR_RESET);
                }
            } else if (game->board[i][j] == AI_CELL_CROSSES) {
                if (is_cursor_here) {
                    // Human stone with cursor: add grey background
                    // printf("%s%s%s", COLOR_X_INVALID, UNICODE_CROSSES, COLOR_RESET);
                    printf("%s%s%s", COLOR_RESET, UNICODE_OCCUPIED, COLOR_RESET);
                } else {
                    // Human stone without cursor: normal red
                    printf("%s%s%s", COLOR_X_NORMAL, UNICODE_CROSSES, COLOR_RESET);
                }
            } else { // AI_CELL_NAUGHTS
                if (is_cursor_here) {
                    // AI stone with cursor: add grey background
                    if (i == game->last_ai_move_x && j == game->last_ai_move_y) {
                        //   printf("%s%s%s", COLOR_O_INVALID, UNICODE_NAUGHTS, COLOR_RESET);
                        printf("%s%s%s", COLOR_RESET, UNICODE_OCCUPIED, COLOR_RESET);
                    } else {
                        // printf("%s%s%s", COLOR_O_INVALID, UNICODE_NAUGHTS, COLOR_RESET);
                        printf("%s%s%s", COLOR_RESET, UNICODE_OCCUPIED, COLOR_RESET);
                    }
                } else {
                    // AI stone without cursor: normal highlighting
                    if (i == game->last_ai_move_x && j == game->last_ai_move_y) {
                        printf("%s%s%s", COLOR_O_LAST_MOVE, UNICODE_NAUGHTS, COLOR_RESET);
                    } else {
                        printf("%s%s%s", COLOR_O_NORMAL, UNICODE_NAUGHTS, COLOR_RESET);
                    }
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
    if (game->current_player == AI_CELL_CROSSES) {
        printf("%s│%s %-*s %s│%s%s\n", prefix, COLOR_YELLOW, action_width + control_width + 2,
                "Current Player : You (X)", COLOR_RESET, COLOR_RESET, COLOR_YELLOW);
    } else {
        printf("%s│%s %-*s %s│%s%s\n", prefix, COLOR_BLUE, action_width + control_width + 2,
                "Current Player : Computer (O)", COLOR_RESET, COLOR_RESET, COLOR_BLUE);
    }

    // Position (convert to 1-based coordinates for display)
    char position_str[32];
    snprintf(position_str, sizeof(position_str),
            "Position       : [ %2d, %2d ]",
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
    char difficulty_str[box_width];
    char difficulty_val[box_width];
    memset(difficulty_str, 0, sizeof(difficulty_str));
    memset(difficulty_val, 0, sizeof(difficulty_val));

    snprintf(difficulty_str, sizeof(difficulty_str), "%sDifficulty     : %-*s", difficulty_color, action_width, difficulty_name);
    printf("%s%s│ %s %s  │\n", prefix, COLOR_RESET, difficulty_str, COLOR_RESET);

    memset(difficulty_str, 0, sizeof(difficulty_str));
    snprintf(difficulty_str, sizeof(difficulty_str), "%sSearch Depth   : %-*d", difficulty_color, action_width, game->max_depth);
    printf("%s%s│ %s %s  │%s\n", prefix, COLOR_RESET, difficulty_str, COLOR_RESET, COLOR_RESET);

    // Separator line
    printf("%s%s│ %-*s %s│%s\n", prefix, COLOR_RESET, box_width - 4, "", COLOR_RESET, COLOR_RESET);

    // Controls header
    printf("%s%s│ %s%-*s %s│\n", prefix, COLOR_RESET, COLOR_BRIGHT_BLUE, box_width - 4, "Controls", COLOR_RESET);

    // Control instructions
    printf("%s%s│ %s%-*s — %s%-*s%s│\n", prefix, COLOR_RESET, COLOR_BRIGHT_YELLOW, control_width, "Arrow Keys", COLOR_GREEN, action_width, "Move cursor", COLOR_RESET);
    printf("%s%s│ %s%-*s — %s%-*s%s│\n", prefix, COLOR_RESET,
            COLOR_BRIGHT_YELLOW, control_width, "Space / Enter", COLOR_GREEN,
            action_width, "Make move", COLOR_RESET);
    if (game->config.enable_undo) {
        printf("%s%s│ %s%-*s — %s%-*s%s│\n", prefix, COLOR_RESET, COLOR_BRIGHT_YELLOW, control_width, "U", COLOR_GREEN, action_width, "Undo last move pair", COLOR_RESET);
    }
    printf("%s%s│ %s%-*s — %s%-*s%s│\n", prefix, COLOR_RESET, COLOR_BRIGHT_YELLOW, control_width, "?", COLOR_GREEN, action_width, "Show game rules", COLOR_RESET);
    printf("%s%s│ %s%-*s — %s%-*s%s│\n", prefix, COLOR_RESET, COLOR_BRIGHT_YELLOW, control_width, "ESC", COLOR_GREEN, action_width, "Quit game", COLOR_RESET);

    printf("%s%s│ %-*s │%s\n", prefix, COLOR_RESET, box_width - 4, " ", COLOR_RESET);

    // AI status message if available
    if (strlen(game->ai_status_message) > 0) {
        printf("%s%s├%-*s┤%s\n", prefix, COLOR_RESET, box_width - 4, "──────────────────────────────────────", COLOR_RESET);

        // Extract clean message without ANSI color codes
        char clean_message[100] = "";
        const char* msg = game->ai_status_message;
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

        printf("%s%s│%s %-*s %s│\n", prefix, COLOR_RESET, COLOR_MAGENTA, 
                box_width - 4, clean_message, COLOR_RESET);
    }

    // Game state messages
    if (game->game_state != GAME_RUNNING) {
        printf("%s%s├%-*s┤%s\n", prefix, COLOR_RESET, box_width - 4, "──────────────────────────────────────", COLOR_RESET);
        printf("%s%s│%s %-*s %s│\n", prefix, COLOR_RESET, COLOR_RESET, 
                box_width - 4, "", COLOR_RESET);

        switch (game->game_state) {
            case GAME_HUMAN_WIN:
                printf("%s%s│ %-*s %s│\n", prefix, COLOR_RESET, 
                        box_width - 4, "Human wins! Great job!", COLOR_RESET);
                break;
            case GAME_AI_WIN:
                printf("%s%s│ %-*s %s│\n", prefix, COLOR_RESET, 
                        box_width - 4, "AI wins! Try again!", COLOR_RESET);
                break;
            case GAME_DRAW:
                printf("%s%s│%s %-*s %s│\n", prefix, COLOR_RESET, COLOR_RESET, 
                        control_width, "The Game is a draw!", COLOR_RESET);
                break;
        }

        // Show timing summary
        char time_summary[100];
        snprintf(time_summary, sizeof(time_summary), 
                "Time: Human: %.1fs | AI: %.1fs", 
                game->total_human_time, game->total_ai_time);
        printf("%s%s│ %-*s %s│\n", prefix, COLOR_RESET, 
                box_width - 4, time_summary, COLOR_RESET);

        printf("%s%s│ %-*s %s│\n", prefix, COLOR_RESET, 
                box_width - 4, "Press any key to exit...", COLOR_RESET);
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

    printf("%s═══════════════════════════════════════════════════════════════════════════════%s\n", COLOR_RESET, COLOR_RESET);
    printf("%s        GOMOKU RULES & HELP (RECOMMENDED TO HAVE 66-LINE TERMINAL)             %s\n", COLOR_RESET, COLOR_RESET);
    printf("%s═══════════════════════════════════════════════════════════════════════════════%s\n", COLOR_RESET, COLOR_RESET);
    printf("\n");

    // Basic objective
    printf("%sOBJECTIVE%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("   Gomoku (Five in a Row) is a strategy game where players take turns placing\n");
    printf("   stones on a board. The goal is to be the first to get five stones in a row\n");
    printf("   (horizontally, vertically, or diagonally).\n\n");

    // Game pieces
    printf("%sGAME PIECES%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("   %s%s%s          — Human Player (Crosses) - You play first\n", COLOR_RED, UNICODE_CROSSES, COLOR_RESET);
    printf("   %s%s%s          — AI Player (Naughts) - Computer opponent\n", COLOR_BLUE, UNICODE_NAUGHTS, COLOR_RESET);
    printf("   %s%s%s          — Current cursor position (available move)\n\n", COLOR_BG_CELL_AVAILABLE, UNICODE_CURSOR, COLOR_RESET);
    printf("   %s %s %s or %s %s %s — Current cursor position (occupied cell)\n\n",
            COLOR_O_INVALID, UNICODE_NAUGHTS, COLOR_RESET,
            COLOR_X_INVALID, UNICODE_CROSSES, COLOR_RESET
          );

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
    printf("   • Horizontal: %s%s%s %s%s%s %s%s%s %s%s%s %s%s%s\n", 
            COLOR_RED, UNICODE_CROSSES, COLOR_RESET,
            COLOR_RED, UNICODE_CROSSES, COLOR_RESET,
            COLOR_RED, UNICODE_CROSSES, COLOR_RESET,
            COLOR_RED, UNICODE_CROSSES, COLOR_RESET,
            COLOR_RED, UNICODE_CROSSES, COLOR_RESET);
    printf("   • Vertical:   Lines going up and down\n");
    printf("   • Diagonal:   Lines going diagonally in any direction\n");
    printf("   • Six or more stones in a row do NOT count as a win (overline rule)\n\n");

    // Basic strategies
    printf("%sBASIC STRATEGIES%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("   %sOffense & Defense:%s Balance creating your own lines with blocking\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("   opponent's attempts to get five in a row.\n\n");

    printf("   %sControl the Center:%s The center of the board provides more\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("   opportunities to create lines in multiple directions.\n\n");

    printf("   %sWatch for Threats:%s An 'open three' (three stones with both ends\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("   open) must be blocked immediately, or it becomes an unstoppable 'open four'.\n\n");

    // Controls reminder
    printf("%sGAME CONTROLS%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("   • Arrow Keys: Move cursor\n");
    printf("   • Space/Enter: Place stone\n");
    printf("   • U: Undo last move pair (human + AI) if enabled\n");
    printf("   • ?: Show this help screen\n");
    printf("   • ESC: Quit game\n\n");

    // CLI options
    printf("%sCOMMAND LINE OPTIONS%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("   -d, --depth N        Search depth (1-10) for AI minimax algorithm\n");
    printf("   -l, --level M        Difficulty: easy, intermediate, hard\n");
    printf("   -t, --timeout T      Move timeout in seconds (optional)\n");
    printf("   -b, --board SIZE     Board size: 15 or 19 (default: 19)\n");
    printf("   -h, --help           Show command line help\n\n");

    printf("%sEXAMPLES%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("   gomoku --level easy --board 15\n");
    printf("   gomoku -d 4 -t 30 -b 19\n");
    printf("   gomoku --level hard --timeout 60\n\n");

    // Game variations
    printf("%sDIFFICULTY LEVELS%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("   • Easy (depth 1):         Quick moves, good for beginners\n");
    printf("   • Intermediate (depth 3): Balanced gameplay, default setting\n");
    printf("   • Hard (depth 4):         Advanced AI, challenging for experts\n\n");

    printf("%s═══════════════════════════════════════════════════════════════════════════════%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("                      %sPress any key to return to game%s\n", COLOR_YELLOW, COLOR_RESET);
    printf("%s═══════════════════════════════════════════════════════════════════════════════%s\n", COLOR_BOLD_BLACK, COLOR_RESET);

    // Wait for any key press
    get_key();
}

void refresh_display(game_state_t *game) {
    clear_screen();
    draw_board(game);
    draw_status(game);
} 
