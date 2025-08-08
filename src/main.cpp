//
//  main.cpp
//  gomoku - Main game orchestrator
//
//  Modern C++23 main function that orchestrates the game using modular components
//

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include "gomoku.hpp"
#include "game.h"
#include "ui.hpp"
#include "ai.h"
#include "cli.hpp"

int main(int argc, char* argv[]) {
    // Initialize random seed for first move randomization
    std::srand(std::time(nullptr));

    // Parse command line arguments
    cli_config_t config = parse_arguments(argc, argv);

    // Handle help or invalid arguments
    if (config.show_help) {
        print_help(argv[0]);
        return 0;
    }

    if (!validate_config(&config)) {
        print_help(argv[0]);
        return 1;
    }

    clear_screen();

    if (!config.skip_welcome) {
        draw_game_header();
    }

    // Initialize game state
    game_state_t *game = init_game(config);
    if (!game) {
        std::cout << "Error: Failed to initialize game\n";
        return 1;
    }

    // Initialize threat matrix for evaluation functions
    gomoku::populate_threat_matrix();

    // Enable raw mode for keyboard input
    enable_raw_mode();

    // Main game loop
    while (game->game_state == static_cast<int>(gomoku::GameState::Running)) {
        // Refresh display
        refresh_display(game);

        if (game->current_player == static_cast<int>(gomoku::Player::Cross)) {
            // Human's turn - start timer if this is a new turn
            static int human_timer_started = 0;
            if (!human_timer_started) {
                start_move_timer(game);
                human_timer_started = 1;
            }

            // Handle user input
            handle_input(game);

            // Reset timer flag when move is made
            if (game->current_player != static_cast<int>(gomoku::Player::Cross)) {
                human_timer_started = 0;
            }
        } else {
            // AI's turn - make move automatically
            int ai_x, ai_y;
            start_move_timer(game);

            // Find best AI move
            find_best_ai_move(game, &ai_x, &ai_y);
            double ai_move_time = end_move_timer(game);

            if (ai_x >= 0 && ai_y >= 0) {
                // Get the number of positions evaluated from the AI status message
                int positions_evaluated = 1; // Default for simple moves
                if (game->move_history_count > 0 && game->ai_history_count > 0) {
                    // Extract from the last AI history entry
                    std::sscanf(game->ai_history[game->ai_history_count - 1], 
                               "%*d | %d positions evaluated", &positions_evaluated);
                }

                // Make the AI move
                make_move(game, ai_x, ai_y, static_cast<int>(gomoku::Player::Naught), ai_move_time, positions_evaluated);

                // Track AI's last move for highlighting
                game->last_ai_move_x = ai_x;
                game->last_ai_move_y = ai_y;
            }
        }
    }

    // Game ended - show final state and wait for input
    if (game->game_state != static_cast<int>(gomoku::GameState::Quit)) {
        refresh_display(game);
        get_key(); // Wait for any key press
    }

    // Cleanup
    cleanup_game(game);
    return 0;
}