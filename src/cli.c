//
//  cli.c
//  gomoku - Command Line Interface module
//
//  Handles command-line argument parsing and help display
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "cli.h"
#include "ansi.h"
#include "gomoku.h"

//===============================================================================
// CLI FUNCTIONS
//===============================================================================

cli_config_t parse_arguments(int argc, char* argv[]) {
    cli_config_t config = {
        .board_size = 19,      // Default board size
        .max_depth = 4,        // Default difficulty (medium)
        .move_timeout = 0,     // No timeout by default
        .show_help = 0,
        .invalid_args = 0,
        .enable_undo = 0,
        .skip_welcome = 0
    };

    // Command line options structure
    static struct option long_options[] = {
        {"depth", required_argument, 0, 'd'},
        {"level", required_argument, 0, 'l'},
        {"timeout", required_argument, 0, 't'},
        {"board", required_argument, 0, 'b'},
        {"help", no_argument, 0, 'h'},
        {"undo", no_argument, 0, 'u'},
        {"skip-welcome", no_argument, 0, 's'},
        {0, 0, 0, 0}};

    int c;
    int option_index = 0;

    // Parse command-line arguments using getopt_long
    while ((c = getopt_long(argc, argv, "d:l:t:b:hus", long_options, &option_index)) != -1) {
        switch (c) {
            case 'd':
                config.max_depth = atoi(optarg);
                if (config.max_depth < 1 || config.max_depth > GAME_DEPTH_LEVEL_MAX) {
                    printf("Error: Search depth must be between 1 and %d\n", GAME_DEPTH_LEVEL_MAX);
                    config.invalid_args = 1;
                }
                if (config.max_depth >= GAME_DEPTH_LEVEL_WARN) {
                    printf("  %s%s%d%s\n  %s%s%s\n", COLOR_YELLOW,
                            "WARNING: Search at or above the depth of ",
                            GAME_DEPTH_LEVEL_WARN, " may be slow without timeout. ",
                            COLOR_BRIGHT_GREEN,
                            "(This message will disappear in 3 seconds.)",
                            COLOR_RESET);
                    sleep(3);
                }

                break;

            case 'l':
                if (strcmp(optarg, "easy") == 0) {
                    config.max_depth = GAME_DEPTH_LEVEL_EASY;
                } else if (strcmp(optarg, "intermediate") == 0) {
                    config.max_depth = GAME_DEPTH_LEVEL_MEDIUM;
                } else if (strcmp(optarg, "hard") == 0) {
                    config.max_depth = GAME_DEPTH_LEVEL_HARD;
                } else {
                    printf("Error: Invalid difficulty level '%s'\n", optarg);
                    printf("Valid options are: easy, intermediate, hard\n\n");
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

            case 'u':
                config.enable_undo = 1;
                break;

            case 's':
                config.skip_welcome = 1;
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

    return config;
}

void print_help(const char* program_name) {
    printf("\n%sNAME%s\n", COLOR_BRIGHT_MAGENTA, COLOR_RESET);
    printf("  %s - an entertaining and engaging five-in-a-row version\n\n", program_name);

    printf("%sFLAGS:%s\n", COLOR_BRIGHT_MAGENTA, COLOR_RESET);
    printf("  %s-d, --depth N%s         The depth of search in the MiniMax algorithm\n", COLOR_YELLOW, COLOR_RESET);
    printf("  %s-l, --level M%s         Can be \"easy\", \"intermediate\", \"hard\"\n", COLOR_YELLOW, COLOR_RESET);
    printf("  %s-t, --timeout T%s       Timeout in seconds that AI (and human)\n", COLOR_YELLOW, COLOR_RESET);
    printf("                        have to make their move, otherwise AI must choose\n");
    printf("                        the best move found so far, while human looses the game.\n");
    printf("  %s-b, --board 15,19%s     Board size. Can be either 19 or 15.\n", COLOR_YELLOW, COLOR_RESET);
    printf("  %s-u, --undo       %s     Enable the Undo feature (disabled by the default).\n", COLOR_YELLOW, COLOR_RESET);
    printf("  %s-s, --skip-welcome%s    Skip the welcome screen.\n", COLOR_YELLOW, COLOR_RESET);
    printf("  %s-h, --help%s            Show this help message\n", COLOR_YELLOW, COLOR_RESET);

    printf("\n%sEXAMPLES:%s\n", COLOR_BRIGHT_MAGENTA, COLOR_RESET);
    printf("  %s%s --level easy --board 15\n", COLOR_YELLOW, program_name);
    printf("  %s%s -d 4 -t 30 -b 19\n", COLOR_YELLOW, program_name);
    printf("  %s%s --level hard --timeout 60\n", COLOR_YELLOW, program_name);

    printf("\n%sDIFFICULTY LEVELS:%s\n", COLOR_BRIGHT_MAGENTA, COLOR_RESET);
    printf("  %seasy%s         - Search depth %d (quick moves, good for beginners)\n", COLOR_GREEN, COLOR_RESET, GAME_DEPTH_LEVEL_EASY);
    printf("  %sintermediate%s - Search depth %d (balanced gameplay, default setting)\n", COLOR_GREEN, COLOR_RESET, GAME_DEPTH_LEVEL_MEDIUM);
    printf("  %shard%s         - Search depth %d (advanced AI, challenging for experts)\n", COLOR_GREEN, COLOR_RESET, GAME_DEPTH_LEVEL_HARD);

    printf("\n%sGAME SYMBOLS:%s\n", COLOR_BRIGHT_MAGENTA, COLOR_RESET);
    printf("  %s%s%s - Human player (crosses)\n", COLOR_RED, UNICODE_CROSSES, COLOR_RESET);
    printf("  %s%s%s - AI player (naughts)\n", COLOR_BLUE, UNICODE_NAUGHTS, COLOR_RESET);
    printf("  %s%s%s - Current cursor (x on an empty cell)\n", COLOR_X_CURSOR, UNICODE_CURSOR, COLOR_RESET);
    printf("  %s%s - Current cursor on an occupied cell\n", UNICODE_OCCUPIED, COLOR_RESET);

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
    return !config->invalid_args;
} 
