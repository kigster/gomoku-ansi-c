//
//  ui.cpp
//  gomoku - Modern C++23 User Interface module for display and input handling
//
//  Handles screen rendering, keyboard input, and user interactions using modern C++
//

#include <iostream>
#include <format>
#include <string>
#include <string_view>
#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include "ui.hpp"
#include "ansi.h"

namespace gomoku::ui {

//===============================================================================
// TERMINAL INPUT IMPLEMENTATION
//===============================================================================

TerminalInput::TerminalInput() {
    enable_raw_mode();
}

TerminalInput::~TerminalInput() {
    if (raw_mode_enabled_) {
        disable_raw_mode();
    }
}

void TerminalInput::enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &original_termios_);
    std::atexit([]() {
        // Use a global flag to check if we need to restore
        static bool restored = false;
        if (!restored) {
            static TerminalInput* instance = nullptr;
            if (instance) {
                instance->disable_raw_mode();
            }
            restored = true;
        }
    });

    struct termios raw = original_termios_;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    raw_mode_enabled_ = true;
}

void TerminalInput::disable_raw_mode() {
    if (raw_mode_enabled_) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios_);
        raw_mode_enabled_ = false;
    }
}

int TerminalInput::get_key() const {
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        if (c == '\033') { // ESC sequence
            std::array<char, 3> seq{};
            if (read(STDIN_FILENO, &seq[0], 1) != 1)
                return c;
            if (read(STDIN_FILENO, &seq[1], 1) != 1)
                return c;

            if (seq[0] == '[') {
                switch (seq[1]) {
                    case 'A': return static_cast<int>(Key::Up);
                    case 'B': return static_cast<int>(Key::Down);
                    case 'C': return static_cast<int>(Key::Right);
                    case 'D': return static_cast<int>(Key::Left);
                }
            }
        } else if (c == '\n' || c == '\r') {
            return static_cast<int>(Key::Enter);
        }
        return c;
    }
    return -1;
}

void TerminalInput::handle_input(game_state_t* game) const {
    int key = get_key();

    switch (key) {
        case static_cast<int>(Key::Up):
            if (game->cursor_x > 0) game->cursor_x--;
            break;
        case static_cast<int>(Key::Down):
            if (game->cursor_x < game->board_size - 1) game->cursor_x++;
            break;
        case static_cast<int>(Key::Left):
            if (game->cursor_y > 0) game->cursor_y--;
            break;
        case static_cast<int>(Key::Right):
            if (game->cursor_y < game->board_size - 1) game->cursor_y++;
            break;
        case static_cast<int>(Key::Space):
        case static_cast<int>(Key::Enter):
            if (game->current_player == static_cast<int>(Player::Cross) && 
                    ::is_valid_move(game->board, game->cursor_x, game->cursor_y, game->board_size)) {
                double move_time = end_move_timer(game);
                make_move(game, game->cursor_x, game->cursor_y, static_cast<int>(Player::Cross), move_time, 0);
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
        case static_cast<int>(Key::Escape):
        case 'Q':
        case 'q':
            game->game_state = static_cast<int>(GameState::Quit);
            break;
        default:
            // Ignore other keys
            break;
    }
}

//===============================================================================
// DISPLAY FUNCTIONS
//===============================================================================

void clear_screen() {
    std::cout << "\033[2J\033[H";
}

void draw_game_header() {
    std::cout << '\n';
    std::cout << std::format(" {}{} {}(v{}{})\n\n", 
                            COLOR_YELLOW, GAME_DESCRIPTION, COLOR_RED, GAME_VERSION, COLOR_RESET);
    std::cout << std::format(" {}{}{}\n\n", 
                            COLOR_BRIGHT_GREEN, GAME_COPYRIGHT, COLOR_RESET);
    std::cout << std::format(" {}{}{}\n", 
                            ESCAPE_CODE_BOLD, COLOR_MAGENTA, "HINT:");
    std::cout << std::format(" {}{}{}\n\n\n", 
                            ESCAPE_CODE_BOLD, COLOR_MAGENTA, GAME_RULES_BRIEF);
    std::cout << std::format(" {}{}{}{}\n\n\n", 
                            COLOR_RESET, COLOR_BRIGHT_CYAN, GAME_RULES_LONG, COLOR_RESET);
    std::cout << std::format("\n\n\n {}{}{}{}\n\n\n\n\n\n\n", 
                            COLOR_YELLOW, ESCAPE_CODE_BOLD, 
                            "Press ENTER to start the game, or CTRL-C to quit...", COLOR_RESET);

    std::cout.flush();
    
    // Wait for user to press ENTER
    std::cin.get();
    clear_screen();
}

void draw_game_history_sidebar(const game_state_t* game, int start_row) {
    // Position cursor to the right of the board
    int sidebar_col = std::clamp(game->board_size * 2 + 12, 50, 50);

    // Draw Game History header
    std::printf(ESCAPE_MOVE_CURSOR_TO, start_row, sidebar_col);
    std::printf("%s%sGame History:%s", COLOR_BOLD_BLACK, COLOR_GREEN, COLOR_RESET);

    std::printf(ESCAPE_MOVE_CURSOR_TO, start_row + 1, sidebar_col);
    std::printf("%sMove Player [Time] (AI positions evaluated)%s",
                COLOR_BOLD_BLACK, COLOR_RESET);

    std::printf(ESCAPE_MOVE_CURSOR_TO, start_row + 2, sidebar_col);
    std::printf("%s", "───────────────────────────────────────────────");

    // Draw move history entries
    int display_start = (0 > (game->move_history_count - 15)) ? 0 : (game->move_history_count - 15);
    for (int i = display_start; i < game->move_history_count; i++) {
        move_history_t move = game->move_history[i];
        char player_symbol = (move.player == static_cast<int>(Player::Cross)) ? 'x' : 'o';
        const char* player_color = (move.player == static_cast<int>(Player::Cross)) ? COLOR_RED : COLOR_BLUE;

        std::array<char, 100> move_line{};
        if (move.player == static_cast<int>(Player::Cross)) {
            // Human move (convert to 1-based coordinates for display)
            std::snprintf(move_line.data(), move_line.size(), 
                         "%s%2d | player %c moved to [%2d, %2d] (in %6.2fs)%s",
                         player_color, i + 1, player_symbol, 
                         ::board_to_display_coord(move.x), ::board_to_display_coord(move.y), 
                         move.time_taken, COLOR_RESET);
        } else {
            // AI move (convert to 1-based coordinates for display)
            std::snprintf(move_line.data(), move_line.size(), 
                         "%s%2d | player %c moved to [%2d, %2d] (in %6.2fs, %3d moves evaluated)%s",
                         player_color, i + 1, player_symbol, 
                         ::board_to_display_coord(move.x), ::board_to_display_coord(move.y), 
                         move.time_taken, move.positions_evaluated, COLOR_RESET);
        }

        std::printf("\033[%d;%dH%s", start_row + 3 + (i - display_start), sidebar_col, move_line.data());
    }
}

void draw_board(const game_state_t* game) {
    std::printf("\n     ");

    // Column numbers with Unicode characters
    for (int j = 0; j < game->board_size; j++) {
        if (j > 9) {    
            std::printf("%s%2s%s ", COLOR_BLUE, get_coordinate_unicode(j - 10).data(), COLOR_RESET);
        } else {
            std::printf("%s%2s%s ", COLOR_GREEN, get_coordinate_unicode(j).data(), COLOR_RESET);
        }
    }
    std::printf("\n");

    constexpr int board_start_row = 0;

    for (int i = 0; i < game->board_size; i++) {
        std::printf("  ");
        if (i > 9) {    
            std::printf("%s%2s%s ", COLOR_BLUE, get_coordinate_unicode(i - 10).data(), COLOR_RESET);
        } else {
            std::printf("%s%2s%s ", COLOR_GREEN, get_coordinate_unicode(i).data(), COLOR_RESET);
        }
        for (int j = 0; j < game->board_size; j++) {
            // Check if cursor is at this position
            bool is_cursor_here = (i == game->cursor_x && j == game->cursor_y);

            std::printf(" "); // Always add the space before the symbol

            // Show appropriate symbol based on cell content
            if (game->board[i][j] == static_cast<int>(Player::Empty)) {
                if (is_cursor_here) {
                    // Empty cell with cursor: show yellow blinking cursor (no background)
                    std::printf("%s%s%s", COLOR_X_CURSOR, UNICODE_CURSOR.data(), COLOR_RESET);
                } else {
                    // Empty cell without cursor: show normal grid intersection
                    std::printf("%s%s%s", COLOR_RESET, UNICODE_EMPTY.data(), COLOR_RESET);
                }
            } else if (game->board[i][j] == static_cast<int>(Player::Cross)) {
                if (is_cursor_here) {
                    // Human stone with cursor: add grey background
                    std::printf("%s%s%s", COLOR_RESET, UNICODE_OCCUPIED.data(), COLOR_RESET);
                } else {
                    // Human stone without cursor: normal red
                    std::printf("%s%s%s", COLOR_X_NORMAL, UNICODE_CROSSES.data(), COLOR_RESET);
                }
            } else { // AI_CELL_NAUGHTS
                if (is_cursor_here) {
                    // AI stone with cursor: add grey background
                    std::printf("%s%s%s", COLOR_RESET, UNICODE_OCCUPIED.data(), COLOR_RESET);
                } else {
                    // AI stone without cursor: normal highlighting
                    if (i == game->last_ai_move_x && j == game->last_ai_move_y) {
                        std::printf("%s%s%s", COLOR_O_LAST_MOVE, UNICODE_NAUGHTS.data(), COLOR_RESET);
                    } else {
                        std::printf("%s%s%s", COLOR_O_NORMAL, UNICODE_NAUGHTS.data(), COLOR_RESET);
                    }
                }
            }
        }
        std::printf("\n");
    }

    // Draw game history sidebar
    draw_game_history_sidebar(game, board_start_row + 2);
}

void draw_status(const game_state_t* game) {
    // Add spacing and lock the box to a position
    std::printf(ESCAPE_MOVE_CURSOR_TO, 24, 1);

    // Box width for the status border
    constexpr int box_width = 19 * 2 + 2;
    constexpr int control_width = 14;
    constexpr int action_width = box_width - control_width - 6;
    constexpr std::string_view prefix = "  ";

    // Top border
    std::printf("%s%s┌", prefix.data(), COLOR_RESET);
    for (int i = 0; i < box_width - 2; i++) {
        std::printf("─");
    }
    std::printf("┐%s\n", COLOR_RESET);

    // Current Player
    if (game->current_player == static_cast<int>(Player::Cross)) {
        std::printf("%s│%s %-*s %s│%s%s\n", prefix.data(), COLOR_YELLOW, action_width + control_width + 2,
                   "Current Player : You (X)", COLOR_RESET, COLOR_RESET, COLOR_YELLOW);
    } else {
        std::printf("%s│%s %-*s %s│%s%s\n", prefix.data(), COLOR_BLUE, action_width + control_width + 2,
                   "Current Player : Computer (O)", COLOR_RESET, COLOR_RESET, COLOR_BLUE);
    }

    // Position (convert to 1-based coordinates for display)
    auto position_str = std::format("Position       : [ {:2d}, {:2d} ]",
                                   ::board_to_display_coord(game->cursor_x),
                                   ::board_to_display_coord(game->cursor_y));

    std::printf("%s%s│ %-*s │\n", prefix.data(), COLOR_RESET, box_width - 4, position_str.c_str());

    // Difficulty
    const char *difficulty_name;
    const char *difficulty_color;
    switch (game->max_depth) {
        case 2: // GAME_DEPTH_LEVEL_EASY
            difficulty_name = "Easy";
            difficulty_color = COLOR_GREEN;
            break;
        case 4: // GAME_DEPTH_LEVEL_MEDIUM
            difficulty_name = "Intermediate";
            difficulty_color = COLOR_YELLOW;
            break;
        case 6: // GAME_DEPTH_LEVEL_HARD
            difficulty_name = "Hard";
            difficulty_color = COLOR_RED;
            break;
        default:
            difficulty_name = "Custom";
            difficulty_color = COLOR_MAGENTA;
    }

    // Difficulty display
    auto difficulty_str = std::format("{}Difficulty     : {}", difficulty_color, difficulty_name);
    std::printf("%s%s│ %s %s  │\n", prefix.data(), COLOR_RESET, difficulty_str.c_str(), COLOR_RESET);

    auto depth_str = std::format("{}Search Depth   : {}", difficulty_color, game->max_depth);
    std::printf("%s%s│ %s %s  │%s\n", prefix.data(), COLOR_RESET, depth_str.c_str(), COLOR_RESET, COLOR_RESET);

    // Separator line
    std::printf("%s%s│ %-*s %s│%s\n", prefix.data(), COLOR_RESET, box_width - 4, "", COLOR_RESET, COLOR_RESET);

    // Controls header
    std::printf("%s%s│ %s%-*s %s│\n", prefix.data(), COLOR_RESET, COLOR_BRIGHT_BLUE, box_width - 4, "Controls", COLOR_RESET);

    // Control instructions
    std::printf("%s%s│ %s%-*s — %s%-*s%s│\n", prefix.data(), COLOR_RESET, COLOR_BRIGHT_YELLOW, control_width, "Arrow Keys", COLOR_GREEN, action_width, "Move cursor", COLOR_RESET);
    std::printf("%s%s│ %s%-*s — %s%-*s%s│\n", prefix.data(), COLOR_RESET,
               COLOR_BRIGHT_YELLOW, control_width, "Space / Enter", COLOR_GREEN,
               action_width, "Make move", COLOR_RESET);
    if (game->config.enable_undo) {
        std::printf("%s%s│ %s%-*s — %s%-*s%s│\n", prefix.data(), COLOR_RESET, COLOR_BRIGHT_YELLOW, control_width, "U", COLOR_GREEN, action_width, "Undo last move pair", COLOR_RESET);
    }
    std::printf("%s%s│ %s%-*s — %s%-*s%s│\n", prefix.data(), COLOR_RESET, COLOR_BRIGHT_YELLOW, control_width, "?", COLOR_GREEN, action_width, "Show game rules", COLOR_RESET);
    std::printf("%s%s│ %s%-*s — %s%-*s%s│\n", prefix.data(), COLOR_RESET, COLOR_BRIGHT_YELLOW, control_width, "ESC", COLOR_GREEN, action_width, "Quit game", COLOR_RESET);

    std::printf("%s%s│ %-*s │%s\n", prefix.data(), COLOR_RESET, box_width - 4, " ", COLOR_RESET);

    // AI status message if available
    if (std::strlen(game->ai_status_message) > 0) {
        std::printf("%s%s├%-*s┤%s\n", prefix.data(), COLOR_RESET, box_width - 4, "──────────────────────────────────────", COLOR_RESET);

        // Extract clean message without ANSI color codes
        std::string clean_message;
        clean_message.reserve(100);
        
        for (const char* msg = game->ai_status_message; *msg && clean_message.size() < 99; ++msg) {
            if (*msg == '\033') {
                // Skip ANSI escape sequence
                while (*msg && *msg != 'm') ++msg;
                if (*msg) ++msg;
            } else {
                clean_message += *msg;
            }
        }

        std::printf("%s%s│%s %-*s %s│\n", prefix.data(), COLOR_RESET, COLOR_MAGENTA, 
                   box_width - 4, clean_message.c_str(), COLOR_RESET);
    }

    // Game state messages
    if (game->game_state != static_cast<int>(GameState::Running)) {
        std::printf("%s%s├%-*s┤%s\n", prefix.data(), COLOR_RESET, box_width - 4, "──────────────────────────────────────", COLOR_RESET);
        std::printf("%s%s│%s %-*s %s│\n", prefix.data(), COLOR_RESET, COLOR_RESET, 
                   box_width - 4, "", COLOR_RESET);

        switch (static_cast<GameState>(game->game_state)) {
            case GameState::HumanWin:
                std::printf("%s%s│ %-*s %s│\n", prefix.data(), COLOR_RESET, 
                           box_width - 4, "Human wins! Great job!", COLOR_RESET);
                break;
            case GameState::AIWin:
                std::printf("%s%s│ %-*s %s│\n", prefix.data(), COLOR_RESET, 
                           box_width - 4, "AI wins! Try again!", COLOR_RESET);
                break;
            case GameState::Draw:
                std::printf("%s%s│%s %-*s %s│\n", prefix.data(), COLOR_RESET, COLOR_RESET, 
                           control_width, "The Game is a draw!", COLOR_RESET);
                break;
            default:
                break;
        }

        // Show timing summary
        auto time_summary = std::format("Time: Human: {:.1f}s | AI: {:.1f}s", 
                                       game->total_human_time, game->total_ai_time);
        std::printf("%s%s│ %-*s %s│\n", prefix.data(), COLOR_RESET, 
                   box_width - 4, time_summary.c_str(), COLOR_RESET);

        std::printf("%s%s│ %-*s %s│\n", prefix.data(), COLOR_RESET, 
                   box_width - 4, "Press any key to exit...", COLOR_RESET);
    }

    // Bottom border
    std::printf("  %s└", COLOR_RESET);
    for (int i = 0; i < box_width - 2; i++) {
        std::printf("─");
    }
    std::printf("┘%s\n", COLOR_RESET);
}

void display_rules() {
    clear_screen();

    std::printf("%s═══════════════════════════════════════════════════════════════════════════════%s\n", COLOR_RESET, COLOR_RESET);
    std::printf("%s        GOMOKU RULES & HELP (RECOMMENDED TO HAVE 66-LINE TERMINAL)             %s\n", COLOR_RESET, COLOR_RESET);
    std::printf("%s═══════════════════════════════════════════════════════════════════════════════%s\n", COLOR_RESET, COLOR_RESET);
    std::printf("\n");

    // Basic objective
    std::printf("%sOBJECTIVE%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    std::printf("   Gomoku (Five in a Row) is a strategy game where players take turns placing\n");
    std::printf("   stones on a board. The goal is to be the first to get five stones in a row\n");
    std::printf("   (horizontally, vertically, or diagonally).\n\n");

    // Game pieces
    std::printf("%sGAME PIECES%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    std::printf("   %s%s%s          — Human Player (Crosses) - You play first\n", COLOR_RED, UNICODE_CROSSES.data(), COLOR_RESET);
    std::printf("   %s%s%s          — AI Player (Naughts) - Computer opponent\n", COLOR_BLUE, UNICODE_NAUGHTS.data(), COLOR_RESET);
    std::printf("   %s%s%s          — Current cursor position (available move)\n\n", COLOR_BG_CELL_AVAILABLE, UNICODE_CURSOR.data(), COLOR_RESET);
    std::printf("   %s %s %s or %s %s %s — Current cursor position (occupied cell)\n\n",
               COLOR_O_INVALID, UNICODE_NAUGHTS.data(), COLOR_RESET,
               COLOR_X_INVALID, UNICODE_CROSSES.data(), COLOR_RESET);

    // Continue with the rest of the help text...
    std::printf("%sHOW TO PLAY%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    std::printf("   1. Crosses (Human) always goes first\n");
    std::printf("   2. Players alternate turns placing one stone per turn\n");
    std::printf("   3. Stones are placed on intersections of the grid lines\n");
    std::printf("   4. Once placed, stones cannot be moved or removed\n");
    std::printf("   5. Win by creating an unbroken line of exactly 5 stones\n\n");

    std::printf("%s═══════════════════════════════════════════════════════════════════════════════%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    std::printf("                      %sPress any key to return to game%s\n", COLOR_YELLOW, COLOR_RESET);
    std::printf("%s═══════════════════════════════════════════════════════════════════════════════%s\n", COLOR_BOLD_BLACK, COLOR_RESET);

    // Wait for any key press
    TerminalInput input;
    [[maybe_unused]] auto key = input.get_key();
}

void refresh_display(const game_state_t* game) {
    clear_screen();
    draw_board(game);
    draw_status(game);
}

} // namespace gomoku::ui

//===============================================================================
// C-COMPATIBLE INTERFACE (for gradual migration)
//===============================================================================

static gomoku::ui::TerminalInput* global_input = nullptr;

extern "C" {

struct termios original_termios;

void enable_raw_mode(void) {
    if (!global_input) {
        global_input = new gomoku::ui::TerminalInput();
    }
}

void disable_raw_mode(void) {
    if (global_input) {
        delete global_input;
        global_input = nullptr;
    }
}

int get_key(void) {
    if (!global_input) {
        global_input = new gomoku::ui::TerminalInput();
    }
    return global_input->get_key();
}

void handle_input(game_state_t *game) {
    if (!global_input) {
        global_input = new gomoku::ui::TerminalInput();
    }
    global_input->handle_input(game);
}

void clear_screen(void) {
    gomoku::ui::clear_screen();
}

void draw_game_header(void) {
    gomoku::ui::draw_game_header();
}

void draw_board(game_state_t *game) {
    gomoku::ui::draw_board(game);
}

void draw_game_history_sidebar(game_state_t *game, int start_row) {
    gomoku::ui::draw_game_history_sidebar(game, start_row);
}

void draw_status(game_state_t *game) {
    gomoku::ui::draw_status(game);
}

void display_rules(void) {
    gomoku::ui::display_rules();
}

void refresh_display(game_state_t *game) {
    gomoku::ui::refresh_display(game);
}

}