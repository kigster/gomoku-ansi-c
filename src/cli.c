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
        .invalid_args = 0
    };
    
    // Command line options structure
    static struct option long_options[] = {
        {"depth",    required_argument, 0, 'd'},
        {"level",    required_argument, 0, 'l'},
        {"timeout",  required_argument, 0, 't'},
        {"board",    required_argument, 0, 'b'},
        {"help",     no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    int c;
    int option_index = 0;
    
    // Parse command-line arguments using getopt_long
    while ((c = getopt_long(argc, argv, "d:l:t:b:h", long_options, &option_index)) != -1) {
        switch (c) {
            case 'd':
                config.max_depth = atoi(optarg);
                if (config.max_depth < 1 || config.max_depth > 10) {
                    printf("Error: Search depth must be between 1 and 10\n");
                    config.invalid_args = 1;
                }
                break;
                
            case 'l':
                if (strcmp(optarg, "easy") == 0) {
                    config.max_depth = 2;
                } else if (strcmp(optarg, "intermediate") == 0) {
                    config.max_depth = 4;
                } else if (strcmp(optarg, "hard") == 0) {
                    config.max_depth = 6;
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
    printf("\n%sNAME%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("  %s - an entertaining and engaging five-in-a-row version\n\n", program_name);
    
    printf("%sFLAGS:%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("  %s-d, --depth N%s      The depth of search in the MiniMax algorithm\n", COLOR_YELLOW, COLOR_RESET);
    printf("  %s-l, --level M%s      Can be \"easy\", \"intermediate\", \"hard\"\n", COLOR_YELLOW, COLOR_RESET);
    printf("  %s-t, --timeout T%s    Timeout in seconds that AI (and human)\n", COLOR_YELLOW, COLOR_RESET);
    printf("                     have to make move, otherwise they lose the game.\n");
    printf("                     This parameter is optional, without it there should\n");
    printf("                     be no constraint on depth of search.\n");
    printf("  %s-b, --board 15,19%s  Board size. Can be either 19 or 15.\n", COLOR_YELLOW, COLOR_RESET);
    printf("  %s-h, --help%s         Show this help message\n", COLOR_YELLOW, COLOR_RESET);
    
    printf("\n%sEXAMPLES:%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("  %s%s --level easy --board 15\n", COLOR_YELLOW, program_name);
    printf("  %s%s -d 4 -t 30 -b 19\n", COLOR_YELLOW, program_name);
    printf("  %s%s --level hard --timeout 60\n", COLOR_YELLOW, program_name);
    
    printf("\n%sDIFFICULTY LEVELS:%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("  %seasy%s         - Search depth 2 (quick moves, good for beginners)\n", COLOR_GREEN, COLOR_RESET);
    printf("  %sintermediate%s - Search depth 4 (balanced gameplay, default setting)\n", COLOR_GREEN, COLOR_RESET);
    printf("  %shard%s         - Search depth 6 (advanced AI, challenging for experts)\n", COLOR_GREEN, COLOR_RESET);
    
    printf("\n%sGAME SYMBOLS:%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("  %s%s%s - Human player (black)\n", COLOR_RED, UNICODE_BLACK, COLOR_RESET);
    printf("  %s%s%s - AI player (white)\n", COLOR_BLUE, UNICODE_WHITE, COLOR_RESET);
    printf("  %s%s%s - Current cursor (bold blinking yellow on empty cells, grey background on occupied cells)\n", COLOR_CURSOR, UNICODE_CURSOR, COLOR_RESET);
    
    printf("\n%sCONTROLS IN GAME:%s\n", COLOR_BOLD_BLACK, COLOR_RESET);
    printf("  Arrow Keys    - Move cursor\n");
    printf("  Space/Enter   - Place stone\n");
    printf("  U             - Undo last move pair\n");
    printf("  ?             - Show detailed game rules\n");
    printf("  ESC           - Quit game\n");
    printf("\n");
}

int validate_config(const cli_config_t *config) {
    return !config->invalid_args;
} 