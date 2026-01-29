//
//  cli.c
//  gomoku - Command Line Interface module
//
//  Handles command-line argument parsing and help display
//

#include "cli.h"
#include "ansi.h"
#include "gomoku.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//===============================================================================
// CLI FUNCTIONS
//===============================================================================

cli_config_t parse_arguments(int argc, char *argv[]) {
  cli_config_t config = {
      .board_size = 19,  // Default board size
      .max_depth = 3,    // Default difficulty (medium)
      .move_timeout = 0, // No timeout by default
      .show_help = 0,
      .invalid_args = 0,
      .enable_undo = 0,
      .skip_welcome = 0,
      .search_radius = 3,                 // Default search radius
      .json_file = "",                    // No JSON output by default
      .replay_file = "",                  // No replay by default
      .replay_wait = 0,                   // Wait for keypress by default
      .player_x_type = PLAYER_TYPE_HUMAN, // X is human by default
      .player_o_type = PLAYER_TYPE_AI,    // O is AI by default
      .depth_x = -1,                      // -1 means use max_depth
      .depth_o = -1,                      // -1 means use max_depth
      .player_x_explicit = 0,             // Track if -x was explicitly set
      .player_o_explicit = 0              // Track if -o was explicitly set
  };

  // Command line options structure
  static struct option long_options[] = {
      {"depth", required_argument, 0, 'd'},
      {"level", required_argument, 0, 'l'},
      {"timeout", required_argument, 0, 't'},
      {"board", required_argument, 0, 'b'},
      {"radius", required_argument, 0, 'r'},
      {"json", required_argument, 0, 'j'},
      {"replay", required_argument, 0, 'p'},
      {"wait", required_argument, 0, 'w'},
      {"help", no_argument, 0, 'h'},
      {"undo", no_argument, 0, 'u'},
      {"skip-welcome", no_argument, 0, 's'},
      {"player-x", required_argument, 0, 'x'},
      {"player-o", required_argument, 0, 'o'},
      {0, 0, 0, 0}};

  int c;
  int option_index = 0;

  // Parse command-line arguments using getopt_long
  while ((c = getopt_long(argc, argv, "d:l:t:b:r:j:p:w:husx:o:", long_options,
                          &option_index)) != -1) {
    switch (c) {
    case 'd':
      // Check for asymmetric depth format: "4:6"
      if (strchr(optarg, ':') != NULL) {
        int depth1, depth2;
        if (sscanf(optarg, "%d:%d", &depth1, &depth2) == 2) {
          if (depth1 < 1 || depth1 > GAME_DEPTH_LEVEL_MAX || depth2 < 1 ||
              depth2 > GAME_DEPTH_LEVEL_MAX) {
            printf("Error: Both depths must be between 1 and %d\n",
                   GAME_DEPTH_LEVEL_MAX);
            config.invalid_args = 1;
          } else {
            config.depth_x = depth1;
            config.depth_o = depth2;
            config.max_depth =
                (depth1 > depth2) ? depth1 : depth2; // For display
          }
        } else {
          printf("Error: Invalid depth format '%s'. Use 'N' or 'N:M'\n",
                 optarg);
          config.invalid_args = 1;
        }
      } else {
        // Single depth for both players
        config.max_depth = atoi(optarg);
        if (config.max_depth < 1 || config.max_depth > GAME_DEPTH_LEVEL_MAX) {
          printf("Error: Search depth must be between 1 and %d\n",
                 GAME_DEPTH_LEVEL_MAX);
          config.invalid_args = 1;
        }
        // Leave depth_x and depth_o as -1 to use max_depth
      }

      if (config.max_depth >= GAME_DEPTH_LEVEL_WARN) {
        printf("  %s%s%d%s\n  %s%s%s\n", COLOR_YELLOW,
               "WARNING: Search at or above the depth of ",
               GAME_DEPTH_LEVEL_WARN, " may be slow without timeout. ",
               COLOR_BRIGHT_GREEN,
               "(This message will disappear in 3 seconds.)", COLOR_RESET);
        sleep(3);
      }

      break;

    case 'l':
      if (strcmp(optarg, "easy") == 0) {
        config.max_depth = GAME_DEPTH_LEVEL_EASY;
      } else if (strcmp(optarg, "medium") == 0 ||
                 strcmp(optarg, "intermediate") == 0) {
        // Support legacy "intermediate" as an alias for "medium"
        config.max_depth = GAME_DEPTH_LEVEL_MEDIUM;
      } else if (strcmp(optarg, "hard") == 0) {
        config.max_depth = GAME_DEPTH_LEVEL_HARD;
      } else {
        printf("Error: Invalid difficulty level '%s'\n", optarg);
        printf("Valid options are: easy, medium, hard\n\n");
        config.invalid_args = 1;
      }
      break;

    case 't':
      config.move_timeout = atoi(optarg);
      if (config.move_timeout < 0) {
        printf("Error: Timeout must be a positive number\n");
        config.invalid_args = 1;
      }
      break;

    case 'b':
      config.board_size = atoi(optarg);
      if (config.board_size != 15 && config.board_size != 19) {
        printf("Error: Board size must be either 15 or 19\n");
        config.invalid_args = 1;
      }
      break;

    case 'r':
      config.search_radius = atoi(optarg);
      if (config.search_radius < 1 || config.search_radius > 5) {
        printf("Error: Search radius must be between 1 and 5\n");
        config.invalid_args = 1;
      }
      break;

    case 'j':
      if (strlen(optarg) >= sizeof(config.json_file)) {
        printf("Error: JSON file path too long\n");
        config.invalid_args = 1;
      } else {
        strncpy(config.json_file, optarg, sizeof(config.json_file) - 1);
        config.json_file[sizeof(config.json_file) - 1] = '\0';
      }
      break;

    case 'p':
      if (strlen(optarg) >= sizeof(config.replay_file)) {
        printf("Error: Replay file path too long\n");
        config.invalid_args = 1;
      } else {
        strncpy(config.replay_file, optarg, sizeof(config.replay_file) - 1);
        config.replay_file[sizeof(config.replay_file) - 1] = '\0';
      }
      break;

    case 'w':
      config.replay_wait = atof(optarg);
      if (config.replay_wait < 0) {
        printf("Error: Wait time must be a positive number\n");
        config.invalid_args = 1;
      }
      break;

    case 'u':
      config.enable_undo = 1;
      break;

    case 's':
      config.skip_welcome = 1;
      break;

    case 'x':
      config.player_x_explicit = 1;
      if (strcmp(optarg, "human") == 0) {
        config.player_x_type = PLAYER_TYPE_HUMAN;
      } else if (strcmp(optarg, "ai") == 0) {
        config.player_x_type = PLAYER_TYPE_AI;
      } else {
        printf("Error: Invalid player type '%s' for -x/--player-x\n", optarg);
        printf("Valid options are: human, ai\n\n");
        config.invalid_args = 1;
      }
      break;

    case 'o':
      config.player_o_explicit = 1;
      if (strcmp(optarg, "human") == 0) {
        config.player_o_type = PLAYER_TYPE_HUMAN;
      } else if (strcmp(optarg, "ai") == 0) {
        config.player_o_type = PLAYER_TYPE_AI;
      } else {
        printf("Error: Invalid player type '%s' for -o/--player-o\n", optarg);
        printf("Valid options are: human, ai\n\n");
        config.invalid_args = 1;
      }
      break;

    case 'h':
      config.show_help = 1;
      break;

    case '?':
      printf("Unknown option or missing argument\n\n");
      config.invalid_args = 1;
      break;

    default:
      config.invalid_args = 1;
      break;
    }
  }

  // Check for non-option arguments
  if (optind < argc) {
    printf("Error: Unexpected arguments: ");
    while (optind < argc) {
      printf("%s ", argv[optind++]);
    }
    printf("\n\n");
    config.invalid_args = 1;
  }

  // If -o human was specified but -x was not, set X to AI
  // This allows "gomoku -o human" to mean "I want to play as O against AI"
  if (config.player_o_explicit && !config.player_x_explicit &&
      config.player_o_type == PLAYER_TYPE_HUMAN) {
    config.player_x_type = PLAYER_TYPE_AI;
  }

  // Similarly, if -x ai was specified but -o was not, set O to human
  // This allows "gomoku -x ai" to mean "AI plays X, I play O"
  if (config.player_x_explicit && !config.player_o_explicit &&
      config.player_x_type == PLAYER_TYPE_AI) {
    config.player_o_type = PLAYER_TYPE_HUMAN;
  }

  return config;
}

void print_help(const char *program_name) {
  printf("\n%sNAME%s\n", COLOR_BRIGHT_MAGENTA, COLOR_RESET);
  printf("  %s%s%s - an entertaining and engaging terminal game with "
         "sophisticated\n",
         COLOR_RED, program_name, COLOR_RESET);
  printf("  computer algorithm that can play against a human player or "
         "itself.\n\n");

  printf("%sVERSION%s\n", COLOR_BRIGHT_MAGENTA, COLOR_RESET);
  printf("  %s%s%s\n\n", COLOR_YELLOW, GAME_VERSION, COLOR_RESET);

  printf("%sGAMEPLAY FLAGS:%s\n", COLOR_BRIGHT_MAGENTA, COLOR_RESET);
  printf("  %s-b, --board 15,19%s     Board size. Can be either 19 or 15.\n",
         COLOR_YELLOW, COLOR_RESET);
  printf("  %s-x, --player-x TYPE%s   Player X type: \"human\" or \"ai\" "
         "(default: human)\n",
         COLOR_YELLOW, COLOR_RESET);
  printf("  %s-o, --player-o TYPE%s   Player O type: \"human\" or \"ai\" "
         "(default: ai)\n",
         COLOR_YELLOW, COLOR_RESET);
  printf("  %s-u, --undo       %s     Enable the Undo feature (disabled by the "
         "default).\n",
         COLOR_YELLOW, COLOR_RESET);
  printf("                        Applies only to when a human player is "
         "involved.\n");
  printf("  %s-s, --skip-welcome%s    Skip the welcome screen.\n", COLOR_YELLOW,
         COLOR_RESET);
  printf("  %s-t, --timeout T%s       Timeout in seconds that AI (and human)\n",
         COLOR_YELLOW, COLOR_RESET);
  printf("                        have to make their move, otherwise AI must "
         "choose\n");
  printf("                        the best move found so far, while human "
         "looses the game.\n\n");

  printf("%sAI PLAYER(s) FLAGS:%s\n", COLOR_BRIGHT_MAGENTA, COLOR_RESET);

  printf(
      "  %s-d, --depth N%s         The depth of search, default is 3. When\n",
      COLOR_YELLOW, COLOR_RESET);

  printf("                        both players are AI players,\n");
  printf("                        use N for both, or N:M for asymmetric depths "
         "(X:0)\n");
  printf("                        Examples: '4' or '4:6'\n");
  printf("  %s-l, --level M%s         Can be \"easy\", \"medium\", \"hard\"\n",
         COLOR_YELLOW, COLOR_RESET);
  printf("                        Maps to depth of 2, 4 and 6.\n");
  printf("  %s-r, --radius 1-5%s      Search radius for move generation "
         "(default: 3).\n",
         COLOR_YELLOW, COLOR_RESET);
  printf("                        Higher values find more moves but run "
         "slower.\n\n");

  printf("%sSPECIAL FLAGS:%s\n", COLOR_BRIGHT_MAGENTA, COLOR_RESET);
  printf("  %s-j, --json FILE%s       Write game results to a JSON file, which "
         "can be replayed.\n",
         COLOR_YELLOW, COLOR_RESET);
  printf("  %s-p, --replay FILE%s     Replay a previously recorded game from a "
         "JSON file.\n",
         COLOR_YELLOW, COLOR_RESET);
  printf("                        By the default, each move requires a "
         "key-press.\n");
  printf("  %s-w, --wait SECS%s       Disable manual key-press and "
         "auto-advance replay\n",
         COLOR_YELLOW, COLOR_RESET);
  printf("                        after waiting SECS after each move.\n");
  printf("  %s-h, --help%s            Show this help message\n\n", COLOR_YELLOW,
         COLOR_RESET);

  printf("\n%sEXAMPLES:%s\n", COLOR_BRIGHT_MAGENTA, COLOR_RESET);
  printf(
      "  %s%s --level easy --board 15%s                # Human vs AI (easy)\n",
      COLOR_YELLOW, program_name, COLOR_RESET);
  printf("  %s%s -x human -o human%s                      # Human vs Human\n",
         COLOR_YELLOW, program_name, COLOR_RESET);
  printf("  %s%s -x ai -o human%s                         # AI vs Human (AI "
         "plays first)\n",
         COLOR_YELLOW, program_name, COLOR_RESET);
  printf("  %s%s -x ai -o ai -d 4:6 --skip-welcome%s      # AI vs AI (X depth "
         "4, O depth 6)\n",
         COLOR_YELLOW, program_name, COLOR_RESET);
  printf("  %s%s -d 4 -t 30 -b 19%s                       # Custom depth and "
         "timeout\n",
         COLOR_YELLOW, program_name, COLOR_RESET);
  printf("  %s%s -p game.json%s                           # Replay a recorded "
         "game\n",
         COLOR_YELLOW, program_name, COLOR_RESET);
  printf("  %s%s -p game.json -w 0.5%s                    # Auto-replay with "
         "0.5s delay\n",
         COLOR_YELLOW, program_name, COLOR_RESET);

  printf("\n%sDIFFICULTY LEVELS:%s\n", COLOR_BRIGHT_MAGENTA, COLOR_RESET);
  printf("  %seasy%s         - Search depth %d (quick moves, good for "
         "beginners)\n",
         COLOR_GREEN, COLOR_RESET, GAME_DEPTH_LEVEL_EASY);
  printf("  %smedium%s       - Search depth %d (balanced gameplay, default "
         "setting)\n",
         COLOR_GREEN, COLOR_RESET, GAME_DEPTH_LEVEL_MEDIUM);
  printf("  %shard%s         - Search depth %d (advanced AI, challenging for "
         "experts)\n",
         COLOR_GREEN, COLOR_RESET, GAME_DEPTH_LEVEL_HARD);

  printf("\n%sGAME SYMBOLS:%s\n", COLOR_BRIGHT_MAGENTA, COLOR_RESET);
  printf("  %s%s%s - Human player (crosses)\n", COLOR_RED, UNICODE_CROSSES,
         COLOR_RESET);
  printf("  %s%s%s - AI player (naughts)\n", COLOR_BLUE, UNICODE_NAUGHTS,
         COLOR_RESET);
  printf("  %s%s%s - Cursor (yellow, matches your piece)\n", COLOR_CURSOR,
         UNICODE_CROSSES, COLOR_RESET);
  printf("  %s%s%s%s - Cursor on occupied cell (yellow background)\n",
         COLOR_X_NORMAL, COLOR_BG_CURSOR_OCCUPIED, UNICODE_CROSSES,
         COLOR_RESET);

  printf("\n%sCONTROLS IN GAME:%s\n", COLOR_BRIGHT_MAGENTA, COLOR_RESET);
  printf("  Arrow Keys    - Move cursor\n");
  printf("  Space/Enter   - Place stone\n");
  printf("  U             - Undo last move pair\n");
  printf("  ?             - Show detailed game rules\n");
  printf("  ESC           - Quit game\n");

  printf("\n%sDEVELOPER INFO:%s\n", COLOR_BRIGHT_MAGENTA, COLOR_RESET);
  printf("  %s%s%s\n", COLOR_BRIGHT_GREEN, GAME_COPYRIGHT, COLOR_RESET);
  printf("  %sVersion %s%s |", COLOR_BRIGHT_MAGENTA, GAME_VERSION, COLOR_RESET);
  printf(" Source: %s%s%s\n", COLOR_BRIGHT_MAGENTA, GAME_URL, COLOR_RESET);
  printf("\n");
}

int validate_config(const cli_config_t *config) {
  if (config->invalid_args) {
    return 0;
  }

  // Validate depths for AI players
  if (config->player_x_type == PLAYER_TYPE_AI ||
      config->player_o_type == PLAYER_TYPE_AI) {
    int effective_depth_x =
        (config->depth_x >= 0) ? config->depth_x : config->max_depth;
    int effective_depth_o =
        (config->depth_o >= 0) ? config->depth_o : config->max_depth;

    if (effective_depth_x < 1 || effective_depth_x > GAME_DEPTH_LEVEL_MAX) {
      printf("Error: Player X AI depth must be between 1 and %d\n",
             GAME_DEPTH_LEVEL_MAX);
      return 0;
    }

    if (effective_depth_o < 1 || effective_depth_o > GAME_DEPTH_LEVEL_MAX) {
      printf("Error: Player O AI depth must be between 1 and %d\n",
             GAME_DEPTH_LEVEL_MAX);
      return 0;
    }
  }

  // Warn if timeout is set for human vs human
  if (config->player_x_type == PLAYER_TYPE_HUMAN &&
      config->player_o_type == PLAYER_TYPE_HUMAN && config->move_timeout > 0) {
    printf("Warning: Timeout is set for Human vs Human mode. ");
    printf("Humans will lose if they don't move within %d seconds.\n\n",
           config->move_timeout);
  }

  // Warn if -w is used without -p
  if (config->replay_wait > 0 && strlen(config->replay_file) == 0) {
    printf("Warning: --wait is ignored without --replay\n\n");
  }

  return 1;
}
