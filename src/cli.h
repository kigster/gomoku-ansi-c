//
//  cli.h
//  gomoku - Command Line Interface module
//
//  Handles command-line argument parsing and help display
//

#ifndef CLI_H
#define CLI_H

//===============================================================================
// CLI CONFIGURATION STRUCTURE
//===============================================================================

/**
 * Structure to hold parsed command line arguments
 */
typedef struct {
    int board_size;      // Board size (15 or 19)
    int max_depth;       // AI search depth
    int move_timeout;    // Move timeout in seconds (0 = no timeout)
    int show_help;       // Whether to show help and exit
    int invalid_args;    // Whether invalid arguments were provided
    int enable_undo;     // Whether to enable undo feature
    int skip_welcome;    // Whether to skip the welcome screen
} cli_config_t;

//===============================================================================
// CLI FUNCTIONS
//===============================================================================

/**
 * Parses command line arguments and returns configuration.
 * 
 * @param argc Number of command line arguments
 * @param argv Array of command line argument strings
 * @return Configuration structure with parsed values
 */
cli_config_t parse_arguments(int argc, char* argv[]);

/**
 * Prints help message with usage information.
 * 
 * @param program_name The name of the program (argv[0])
 */
void print_help(const char* program_name);

/**
 * Validates the parsed configuration for consistency.
 * 
 * @param config The configuration to validate
 * @return 1 if valid, 0 if invalid
 */
int validate_config(const cli_config_t *config);

#endif // CLI_H 