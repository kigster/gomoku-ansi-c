//
//  main.c
//  gomoku - Main game orchestrator
//
//  Simple main function that orchestrates the game using modular components
//

#include "ai.h"
#include "cli.h"
#include "game.h"
#include "gomoku.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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

/**
 * Runs replay mode - loads and displays a previously recorded game.
 * @param config The CLI configuration with replay_file and replay_wait
 * @return 0 on success, 1 on error
 */
static int run_replay_mode(cli_config_t *config) {
  // Load the game data
  replay_data_t replay;
  if (!load_game_json(config->replay_file, &replay)) {
    printf("Error: Failed to load replay file '%s'\n", config->replay_file);
    return 1;
  }

  if (replay.move_count == 0) {
    printf("Error: No moves found in replay file\n");
    return 1;
  }

  // Create a minimal config for the game state
  cli_config_t game_config = *config;
  game_config.board_size = replay.board_size;
  game_config.player_x_type = PLAYER_TYPE_HUMAN; // Doesn't matter for replay
  game_config.player_o_type = PLAYER_TYPE_HUMAN;

  // Initialize game state
  game_state_t *game = init_game(game_config);
  if (!game) {
    printf("Error: Failed to initialize game for replay\n");
    return 1;
  }
  game->replay_mode = 1; // Disable cursor in replay mode

  clear_screen();

  // Enable raw mode for keyboard input
  enable_raw_mode();

  // Show initial empty board
  refresh_display(game);

  // Display replay info
  printf("\n  Replaying game from: %s\n", config->replay_file);
  printf("  Total moves: %d | Winner: %s\n", replay.move_count, replay.winner);
  if (config->replay_wait > 0) {
    printf("  Auto-advance: %.1fs delay\n", config->replay_wait);
  } else {
    printf("  Press any key for next move, 'q' to quit\n");
  }

  // Wait for initial keypress or delay
  if (config->replay_wait > 0) {
    usleep((useconds_t)(config->replay_wait * 1000000));
  } else {
    int key = get_key();
    if (key == 'q' || key == 'Q' || key == 27) { // q or ESC
      cleanup_game(game);
      return 0;
    }
  }

  // Replay each move
  for (int i = 0; i < replay.move_count; i++) {
    move_history_t *move = &replay.moves[i];

    // Place the stone on the board
    game->board[move->x][move->y] = move->player;
    game->current_player = move->player;
    game->last_ai_move_x = move->x;
    game->last_ai_move_y = move->y;
    game->cursor_x = move->x;
    game->cursor_y = move->y;

    // Add to move history for display
    if (game->move_history_count < MAX_MOVE_HISTORY) {
      game->move_history[game->move_history_count] = *move;
      game->move_history_count++;
    }

    // Check for winner
    if (move->is_winner) {
      if (move->player == AI_CELL_CROSSES) {
        game->game_state = GAME_HUMAN_WIN;
      } else {
        game->game_state = GAME_AI_WIN;
      }
    }

    // Refresh display
    refresh_display(game);

    // Show move info
    const char *player_str = (move->player == AI_CELL_CROSSES) ? "X" : "O";
    printf("\n  Move %d/%d: %s at [%d, %d]", i + 1, replay.move_count,
           player_str, move->x, move->y);
    if (move->time_taken > 0) {
      printf(" (%.3f ms)", move->time_taken * 1000.0);
    }
    if (move->is_winner) {
      printf(" ** WINNER **");
    }
    printf("\n");

    // Wait for next move
    if (i < replay.move_count - 1) { // Don't wait after last move
      if (config->replay_wait > 0) {
        usleep((useconds_t)(config->replay_wait * 1000000));
      } else {
        int key = get_key();
        if (key == 'q' || key == 'Q' || key == 27) { // q or ESC
          break;
        }
      }
    }
  }

  // Final message
  printf("\n  Replay complete. Press any key to exit.\n");
  get_key();

  cleanup_game(game);
  return 0;
}

int main(int argc, char *argv[]) {
  // Initialize random seed for first move randomization
  srand(time(NULL));

  // Parse command line arguments
  cli_config_t config = parse_arguments(argc, argv);

  // Handle help or invalid arguments
  if (config.show_help) {
    print_help(GAME_NAME);
    return 0;
  }

  if (!validate_config(&config)) {
    print_help(argv[0]);
    return 1;
  }

  // Check for replay mode
  if (strlen(config.replay_file) > 0) {
    return run_replay_mode(&config);
  }

  if (!config.headless) {
    clear_screen();

    if (!config.skip_welcome) {
      draw_game_header();
    }
  }

  // Initialize game state
  game_state_t *game = init_game(config);
  if (!game) {
    printf("Error: Failed to initialize game\n");
    return 1;
  }

  // Initialize threat matrix for evaluation functions
  populate_threat_matrix();

  // Enable raw mode for keyboard input (skip in headless mode)
  if (!config.headless) {
    enable_raw_mode();
  }

  // Main game loop
  while (game->game_state == GAME_RUNNING) {
    // Refresh display (skip in headless mode)
    if (!config.headless) {
      refresh_display(game);
    }

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

      find_best_ai_move(game, &ai_x, &ai_y, NULL);

      game->max_depth = saved_depth; // Restore

      double ai_move_time = end_move_timer(game);

      if (ai_x >= 0 && ai_y >= 0) {
        // Get the number of positions evaluated from the AI status message
        int positions_evaluated = 1; // Default for simple moves
        if (game->move_history_count > 0 && game->ai_history_count > 0) {
          // Extract from the last AI history entry
          sscanf(game->ai_history[game->ai_history_count - 1],
                 "%*d | %d positions evaluated", &positions_evaluated);
        }

        // Calculate threat scores for JSON output
        int own_score = evaluate_threat_fast(
            game->board, ai_x, ai_y, game->current_player, game->board_size);
        int opp_score = evaluate_threat_fast(game->board, ai_x, ai_y,
                                             other_player(game->current_player),
                                             game->board_size);

        // Make the AI move
        make_move(game, ai_x, ai_y, game->current_player, ai_move_time,
                  positions_evaluated, own_score, opp_score);

        // Track AI's last move for highlighting
        game->last_ai_move_x = ai_x;
        game->last_ai_move_y = ai_y;

        // If the next player is human, position cursor near the last move
        if (game->game_state == GAME_RUNNING) {
          int next_player_index =
              (game->current_player == AI_CELL_CROSSES) ? 0 : 1;
          if (game->player_type[next_player_index] == PLAYER_TYPE_HUMAN) {
            position_cursor_near_last_move(game);
          }
        }
      }
    }
  }

  // Game ended - show final state and wait for input (skip in headless mode)
  if (game->game_state != GAME_QUIT && !config.headless) {
    refresh_display(game);
    get_key(); // Wait for any key press
  }

  // Write JSON output if requested
  if (strlen(config.json_file) > 0) {
    if (write_game_json(game, config.json_file)) {
      if (!config.headless) {
        printf("Game saved to %s\n", config.json_file);
      }
    } else {
      fprintf(stderr, "Error: Failed to write JSON to %s\n", config.json_file);
    }
  }

  // Cleanup
  cleanup_game(game);
  return 0;
}
