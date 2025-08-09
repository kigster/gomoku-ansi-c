//
//  ui.hpp
//  gomoku - Modern C++23 User Interface module for display and input handling
//
//  Handles screen rendering, keyboard input, and user interactions using modern C++
//

#pragma once

#include <termios.h>
#include <string>
#include <string_view>
#include "gomoku.hpp"
#include "game.h"
#include "ansi.h"

namespace gomoku::ui {

//===============================================================================
// INPUT CONSTANTS
//===============================================================================

enum class Key : int {
    Up = 72,
    Down = 80,
    Left = 75,
    Right = 77,
    Enter = 13,
    Space = 32,
    Escape = 27,
    Tab = 9
};

//===============================================================================
// DISPLAY CONSTANTS
//===============================================================================

inline constexpr std::string_view UNICODE_EMPTY = "·";

//===============================================================================
// INPUT HANDLING CLASS
//===============================================================================

/**
 * Modern C++23 Terminal Input Handler
 */
class TerminalInput {
public:
    TerminalInput();
    ~TerminalInput();

    // Disable copy/move to ensure RAII safety
    TerminalInput(const TerminalInput&) = delete;
    TerminalInput& operator=(const TerminalInput&) = delete;
    TerminalInput(TerminalInput&&) = delete;
    TerminalInput& operator=(TerminalInput&&) = delete;

    /**
     * Gets a single keypress from the user.
     * @return Key code or -1 on error
     */
    [[nodiscard]] int get_key() const;

    /**
     * Handles user input and updates game state accordingly.
     * @param game The game state
     */
    void handle_input(game_state_t* game) const;

private:
    struct termios original_termios_;
    bool raw_mode_enabled_ = false;

    void enable_raw_mode();
    void disable_raw_mode();
};

//===============================================================================
// DISPLAY FUNCTIONS
//===============================================================================

/**
 * Clears the screen.
 */
void clear_screen();

/**
 * Draws the game header with title and instructions.
 */
void draw_game_header();

/**
 * Draws the game board with current stone positions and cursor.
 * @param game The game state
 */
void draw_board(const game_state_t* game);

/**
 * Draws the game history sidebar.
 * @param game The game state
 * @param start_row Starting row position for the sidebar
 */
void draw_game_history_sidebar(const game_state_t* game, int start_row);

/**
 * Draws the status panel with game information and controls.
 * @param game The game state
 */
void draw_status(const game_state_t* game);

/**
 * Displays the game rules in a formatted screen.
 */
void display_rules();

/**
 * Refreshes the entire game display.
 * @param game The game state
 */
void refresh_display(const game_state_t* game);

} // namespace gomoku::ui

//===============================================================================
// C-COMPATIBLE INTERFACE (for gradual migration)
//===============================================================================

extern "C" {
    extern struct termios original_termios;
    void enable_raw_mode(void);
    void disable_raw_mode(void);
    int get_key(void);
    void handle_input(game_state_t *game);
    void clear_screen(void);
    void draw_game_header(void);
    void draw_board(game_state_t *game);
    void draw_game_history_sidebar(game_state_t *game, int start_row);
    void draw_status(game_state_t *game);
    void display_rules(void);
    void refresh_display(game_state_t *game);
}