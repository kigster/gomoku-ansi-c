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
#include "board.h"

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
            if (game->current_player == AI_CELL_BLACK && 
                is_valid_move(game->board, game->cursor_x, game->cursor_y, game->board_size)) {
                double move_time = end_move_timer(game);
                make_move(game, game->cursor_x, game->cursor_y, AI_CELL_BLACK, move_time, 0);
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
    // Calculate centering
    const char* title = "\n  GOMOKU, also known as Five in a Row.\n";
    const char* copyright = " ASCII Version © 2025 Konstantin Gredeskoul, MIT License";
    const char* description = " In this version, human player always starts (its an advantage) with the crosses.\n  The AI player uses a minimax algorithm with alpha-beta pruning to make its moves.";
    const char* rules = " Use arrows to move around, Enter or Space to make a move, U to undo last move pair, \n  ? to show game rules, ESC to quit game.";
    
    printf(" %s%s%s", COLOR_YELLOW, title, COLOR_RESET);
    printf(" %s%s%s\n", COLOR_BRIGHT_GREEN, copyright, COLOR_RESET);
    printf(" %s\n", COLOR_GREEN);
    printf(" %s\n", description);
    printf(" %s\n", COLOR_YELLOW);
    printf(" %s%s%s\n", COLOR_BLUE, rules, COLOR_RESET);
    printf(" %s\n", COLOR_RESET);
    printf(" %s", COLOR_RESET);
    printf("\n\n");
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
    printf("\033[%d;%dH%s%sGame History:%s", start_row, sidebar_col, COLOR_BOLD_BLACK, COLOR_GREEN, COLOR_RESET);
    printf("\033[%d;%dH%sMove Player [Time] (AI positions evaluated)%s", start_row + 1, sidebar_col, COLOR_BOLD_BLACK, COLOR_RESET);
    printf("\033[%d;%dH%s", start_row + 2, sidebar_col, "───────────────────────────────────────────────");
    
    // Draw move history entries
    int display_start = max(0, game->move_history_count - 15); // Show last 15 moves
    for (int i = display_start; i < game->move_history_count; i++) {
        move_history_t move = game->move_history[i];
        char player_symbol = (move.player == AI_CELL_BLACK) ? 'x' : 'o';
        const char* player_color = (move.player == AI_CELL_BLACK) ? COLOR_RED : COLOR_BLUE;
        
        char move_line[100];
        if (move.player == AI_CELL_BLACK) {
            // Human move (convert to 1-based coordinates for display)
            snprintf(move_line, sizeof(move_line), 
                     "%s%2d | player %c moved to [%d, %d] (in %.2fs)%s",
                     player_color, i + 1, player_symbol, 
                     board_to_display_coord(move.x), board_to_display_coord(move.y), 
                     move.time_taken, COLOR_RESET);
        } else {
            // AI move (convert to 1-based coordinates for display)
            snprintf(move_line, sizeof(move_line), 
                     "%s%2d | player %c moved to [%d, %d] (in %.2fs, %d moves evaluated)%s",
                     player_color, i + 1, player_symbol, 
                     board_to_display_coord(move.x), board_to_display_coord(move.y), 
                     move.time_taken, move.positions_evaluated, COLOR_RESET);
        }
        
        printf("\033[%d;%dH%s", start_row + 3 + (i - display_start), sidebar_col, move_line);
    }
}

void draw_board(game_state_t *game) {
    draw_game_header();

    printf("      ");

    // Column numbers with Unicode characters
    for (int j = 0; j < game->board_size; j++) {
        if (j > 9) {    
            printf("%s%2s%s ", COLOR_BLUE, get_coordinate_unicode(j - 10), COLOR_RESET);
        } else {
            printf("%s%2s%s ", COLOR_GREEN, get_coordinate_unicode(j), COLOR_RESET);
        }
    }
    printf("\n");

    int board_start_row = 10; // After header (4 lines) + column numbers (1 line) + buffer (1 line)

    for (int i = 0; i < game->board_size; i++) {
        printf("   ");
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
                    printf("%s%s%s", COLOR_CURSOR, UNICODE_CURSOR, COLOR_RESET);
                } else {
                    // Empty cell without cursor: show normal grid intersection
                    printf("%s%s%s", COLOR_BG_BLACK, UNICODE_EMPTY, COLOR_RESET);
                }
            } else if (game->board[i][j] == AI_CELL_BLACK) {
                if (is_cursor_here) {
                    // Human stone with cursor: add grey background
                    printf("%s%s%s%s", COLOR_BG_GREY, COLOR_RED, UNICODE_BLACK, COLOR_RESET);
                } else {
                    // Human stone without cursor: normal red
                    printf("%s%s%s", COLOR_RED, UNICODE_BLACK, COLOR_RESET);
                }
            } else { // AI_CELL_WHITE
                if (is_cursor_here) {
                    // AI stone with cursor: add grey background
                    if (i == game->last_ai_move_x && j == game->last_ai_move_y) {
                        printf("%s%s%s%s", COLOR_BG_GREY, COLOR_BRIGHT_BLUE, UNICODE_WHITE, COLOR_RESET);
                    } else {
                        printf("%s%s%s%s", COLOR_BG_GREY, COLOR_BLUE, UNICODE_WHITE, COLOR_RESET);
                    }
                } else {
                    // AI stone without cursor: normal highlighting
                    if (i == game->last_ai_move_x && j == game->last_ai_move_y) {
                        printf("%s%s%s", COLOR_BRIGHT_BLUE, UNICODE_WHITE, COLOR_RESET);
                    } else {
                        printf("%s%s%s", COLOR_BLUE, UNICODE_WHITE, COLOR_RESET);
                    }
                }
            }
        }
        printf("\n");
    }
    
    // Draw game history sidebar
    draw_game_history_sidebar(game, board_start_row + 4);
}

void draw_status(game_state_t *game) {
    // Add spacing and lock the box to a position
    printf("\033[36;1H");
    printf("\033[s");
    
    // Box width for the status border
    const int box_width = 40;
    
    // Top border
    printf("%s┌", COLOR_BOLD_BLACK);
    for (int i = 0; i < box_width - 2; i++) {
        printf("─");
    }
    printf("┐%s\n", COLOR_RESET);
    
    // Current Player
    if (game->current_player == AI_CELL_BLACK) {
        printf("%s│ %-*s %s│%s%s\n", COLOR_YELLOW, box_width - 4,
               "Current Player : X (Human)", COLOR_BOLD_BLACK, COLOR_RESET, COLOR_YELLOW);
    } else {
        printf("%s│ %-*s %s│%s%s\n", COLOR_BLUE, box_width - 4,
               "Current Player : O (AI)", COLOR_BOLD_BLACK, COLOR_RESET, COLOR_BLUE);
    }
    
    // Position (convert to 1-based coordinates for display)
    char position_str[32];
    snprintf(position_str, sizeof(position_str),
             "Position       : [%d, %d]", 
             board_to_display_coord(game->cursor_x), 
             board_to_display_coord(game->cursor_y));
    printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
           box_width - 4, position_str, COLOR_BOLD_BLACK, COLOR_RESET);
    
    // Difficulty
    const char* difficulty_name;
    switch (game->max_depth) {
        case 2: difficulty_name = "Easy"; break;
        case 4: difficulty_name = "Medium"; break;
        case 7: difficulty_name = "Hard"; break;
        default: difficulty_name = "Custom"; break;
    }
    char difficulty_str[32];
    snprintf(difficulty_str, sizeof(difficulty_str),
             "Difficulty     : %s (depth %d)", difficulty_name, game->max_depth);
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
           box_width - 4, " Arrow Keys    - Move cursor", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
           box_width - 4, " Space / Enter - Make move", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
           box_width - 4, " U             - Undo last move pair", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
           box_width - 4, " ?             - Show game rules", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
           box_width - 4, " ESC           - Quit game", COLOR_BOLD_BLACK, COLOR_RESET);

    printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
           box_width - 4, "", COLOR_BOLD_BLACK, COLOR_RESET);
         
    // AI status message if available
    if (strlen(game->ai_status_message) > 0) {
        printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
               box_width - 4, "", COLOR_BOLD_BLACK, COLOR_RESET);
        
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
        
        printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
               box_width - 4, clean_message, COLOR_BOLD_BLACK, COLOR_RESET);
    }

    // Game state messages
    if (game->game_state != GAME_RUNNING) {
        printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
               box_width - 4, "", COLOR_BOLD_BLACK, COLOR_RESET);
        
        switch (game->game_state) {
        case GAME_HUMAN_WIN:
            printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_YELLOW, 
                   box_width - 4, "Human wins! Great job!", COLOR_BOLD_BLACK, COLOR_RESET);
            break;
        case GAME_AI_WIN:
            printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_BLUE, 
                   box_width - 4, "AI wins! Try again!", COLOR_BOLD_BLACK, COLOR_RESET);
            break;
        case GAME_DRAW:
            printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
                   box_width - 4, "The Game is a draw!", COLOR_BOLD_BLACK, COLOR_RESET);
            break;
        }
        
        // Show timing summary
        char time_summary[100];
        snprintf(time_summary, sizeof(time_summary), 
                 "Time spent - Human: %.1fs, AI: %.1fs", 
                 game->total_human_time, game->total_ai_time);
        printf("%s│%s %-*s %s│%s\n", COLOR_BOLD_BLACK, COLOR_RESET, 
               box_width - 4, time_summary, COLOR_BOLD_BLACK, COLOR_RESET);
        
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

void display_rules(void) {
    clear_screen();
    
    printf("%s═══════════════════════════════════════════════════════════════════════════════%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("%s                             GOMOKU RULES & HELP                               %s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("%s═══════════════════════════════════════════════════════════════════════════════%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("\n");
    
    // Basic objective
    printf("%sOBJECTIVE%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("   Gomoku (Five in a Row) is a strategy game where players take turns placing\n");
    printf("   stones on a board. The goal is to be the first to get five stones in a row\n");
    printf("   (horizontally, vertically, or diagonally).\n\n");
    
    // Game pieces
    printf("%sGAME PIECES%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("   %s%s%s Human Player (Black) - You play first\n", COLOR_RED, UNICODE_BLACK, COLOR_RESET);
    printf("   %s%s%s AI Player (White) - Computer opponent\n", COLOR_BLUE, UNICODE_WHITE, COLOR_RESET);
    printf("   %s%s%s Current cursor position\n\n", COLOR_CURSOR, UNICODE_CURSOR, COLOR_RESET);
    
    // How to play
    printf("%sHOW TO PLAY%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("   1. Black (Human) always goes first\n");
    printf("   2. Players alternate turns placing one stone per turn\n");
    printf("   3. Stones are placed on intersections of the grid lines\n");
    printf("   4. Once placed, stones cannot be moved or removed\n");
    printf("   5. Win by creating an unbroken line of exactly 5 stones\n\n");
    
    // Winning conditions
    printf("%sWINNING CONDITIONS%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
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
    printf("   • U: Undo last move pair (human + AI)\n");
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
    printf("   • Easy (depth 2):         Quick moves, good for beginners\n");
    printf("   • Intermediate (depth 4): Balanced gameplay, default setting\n");
    printf("   • Hard (depth 6):         Advanced AI, challenging for experts\n\n");
    
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