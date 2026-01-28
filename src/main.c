//
//  main.c
//  gomoku - Main game orchestrator
//
//  Simple main function that orchestrates the game using modular components
//

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "gomoku.h"
#include "game.h"
#include "ui.h"
#include "ai.h"
#include "cli.h"

//===============================================================================
// HELPER FUNCTIONS
//===============================================================================

/**
 * Returns the player type for a given player constant.
 * @param game The game state
 * @param player AI_CELL_CROSSES or AI_CELL_NAUGHTS
 * @return PLAYER_TYPE_HUMAN or PLAYER_TYPE_AI
 */
static player_type_t get_player_type(game_state_t *game, int player) {
    // CROSSES=1 maps to index 0, NAUGHTS=-1 maps to index 1
    int index = (player == AI_CELL_CROSSES) ? 0 : 1;
    return game->player_type[index];
}

/**
 * Returns the AI depth for a given player constant.
 * @param game The game state
 * @param player AI_CELL_CROSSES or AI_CELL_NAUGHTS
 * @return Depth for this player
 */
static int get_player_depth(game_state_t *game, int player) {
    int index = (player == AI_CELL_CROSSES) ? 0 : 1;
    return game->depth_for_player[index];
}

int main(int argc, char* argv[]) {
    // Initialize random seed for first move randomization
    srand(time(NULL));

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
        printf("Error: Failed to initialize game\n");
        return 1;
    }

    // Initialize threat matrix for evaluation functions
    populate_threat_matrix();

    // Enable raw mode for keyboard input
    enable_raw_mode();

    // Main game loop
    while (game->game_state == GAME_RUNNING) {
        // Refresh display
        refresh_display(game);

        player_type_t current_type = get_player_type(game, game->current_player);

        if (current_type == PLAYER_TYPE_HUMAN) {
            // Human's turn - start timer if this is a new turn
            static int human_timer_started = 0;
            static int last_human_player = 0;

            // Reset timer if we switched to a different human player
            if (last_human_player != game->current_player) {
                human_timer_started = 0;
                last_human_player = game->current_player;
            }

            if (!human_timer_started) {
                start_move_timer(game);
                human_timer_started = 1;
            }

            // Handle user input
            handle_input(game);

            // Reset timer flag when move is made (player changed)
            if (game->current_player != last_human_player) {
                human_timer_started = 0;
            }
        } else {
            // AI's turn - make move automatically
            int ai_x, ai_y;
            start_move_timer(game);

            // Find best AI move with player-specific depth
            int saved_depth = game->max_depth;
            game->max_depth = get_player_depth(game, game->current_player);

            find_best_ai_move(game, &ai_x, &ai_y);

            game->max_depth = saved_depth;  // Restore

            double ai_move_time = end_move_timer(game);

            if (ai_x >= 0 && ai_y >= 0) {
                // Get the number of positions evaluated from the AI status message
                int positions_evaluated = 1; // Default for simple moves
                if (game->move_history_count > 0 && game->ai_history_count > 0) {
                    // Extract from the last AI history entry
                    sscanf(game->ai_history[game->ai_history_count - 1],
                            "%*d | %d positions evaluated", &positions_evaluated);
                }

                // Make the AI move
                make_move(game, ai_x, ai_y, game->current_player, ai_move_time, positions_evaluated);

                // Track AI's last move for highlighting
                game->last_ai_move_x = ai_x;
                game->last_ai_move_y = ai_y;

                // If the next player is human, position cursor near the last move
                if (game->game_state == GAME_RUNNING) {
                    int next_player_index = (game->current_player == AI_CELL_CROSSES) ? 0 : 1;
                    if (game->player_type[next_player_index] == PLAYER_TYPE_HUMAN) {
                        position_cursor_near_last_move(game);
                    }
                }
            }
        }
    }

    // Game ended - show final state and wait for input
    if (game->game_state != GAME_QUIT) {
        refresh_display(game);
        get_key(); // Wait for any key press
    }

    // Cleanup
    cleanup_game(game);
    return 0;
}
