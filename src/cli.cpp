//
//  cli.cpp  
//  gomoku - Modern C++23 Command Line Interface module
//
//  Handles command-line argument parsing and help display using modern C++
//

#include <iostream>
#include <format>
#include <string>
#include <string_view>
#include <span>
#include <expected>
#include <unordered_map>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <getopt.h>
#include "cli.hpp"
#include "ansi.h"

namespace gomoku::cli {

//===============================================================================
// CONFIG VALIDATION
//===============================================================================

std::expected<void, std::string> Config::validate() const {
    if (board_size != 15 && board_size != 19) {
        return std::unexpected("Board size must be either 15 or 19");
    }
    
    if (max_depth < 1 || max_depth > 10) {
        return std::unexpected("Search depth must be between 1 and 10");
    }
    
    if (move_timeout < 0) {
        return std::unexpected("Timeout must be a positive number");
    }
    
    return {};
}

auto Config::to_c_struct() const {
    return cli_config_t{
        .board_size = board_size,
        .max_depth = max_depth,
        .move_timeout = move_timeout,
        .show_help = show_help ? 1 : 0,
        .invalid_args = 0,
        .enable_undo = enable_undo ? 1 : 0,
        .skip_welcome = skip_welcome ? 1 : 0
    };
}

//===============================================================================
// ARGUMENT PARSING
//===============================================================================

std::expected<Config, ParseError> parse_arguments_modern(std::span<const char*> args) {
    Config config{};
    
    if (args.empty()) {
        return config; // Default configuration
    }
    
    // Convert span to argc/argv for getopt compatibility
    int argc = static_cast<int>(args.size());
    char** argv = const_cast<char**>(args.data());
    
    // Reset getopt state
    optind = 1;
    
    static constexpr std::array long_options = {
        option{"depth", required_argument, nullptr, 'd'},
        option{"level", required_argument, nullptr, 'l'},
        option{"timeout", required_argument, nullptr, 't'},
        option{"board", required_argument, nullptr, 'b'},
        option{"help", no_argument, nullptr, 'h'},
        option{"undo", no_argument, nullptr, 'u'},
        option{"skip-welcome", no_argument, nullptr, 's'},
        option{nullptr, 0, nullptr, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "d:l:t:b:hus", 
                           const_cast<option*>(long_options.data()), &option_index)) != -1) {
        switch (c) {
            case 'd': {
                config.max_depth = std::atoi(optarg);
                if (config.max_depth < 1 || config.max_depth > 10) {
                    return std::unexpected(ParseError::InvalidDepth);
                }
                if (config.max_depth >= 7) {
                    std::cout << std::format("{}{}WARNING: Search at or above depth {} may be slow{}\n",
                                           COLOR_YELLOW, ESCAPE_CODE_BOLD, 7, COLOR_RESET);
                    std::cout << std::format("{}(This message will disappear in 3 seconds.){}\n",
                                           COLOR_BRIGHT_GREEN, COLOR_RESET);
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                }
                break;
            }
            
            case 'l': {
                const std::unordered_map<std::string_view, int> difficulty_map = {
                    {"easy", 2},
                    {"intermediate", 4},
                    {"hard", 6}
                };
                
                std::string_view level{optarg};
                if (auto it = difficulty_map.find(level); it != difficulty_map.end()) {
                    config.max_depth = it->second;
                } else {
                    return std::unexpected(ParseError::InvalidArgument);
                }
                break;
            }
            
            case 't': {
                config.move_timeout = std::atoi(optarg);
                if (config.move_timeout < 0) {
                    return std::unexpected(ParseError::InvalidTimeout);
                }
                break;
            }
            
            case 'b': {
                config.board_size = std::atoi(optarg);
                if (config.board_size != 15 && config.board_size != 19) {
                    return std::unexpected(ParseError::InvalidBoardSize);
                }
                break;
            }
            
            case 'u':
                config.enable_undo = true;
                break;
                
            case 's':
                config.skip_welcome = true;
                break;
                
            case 'h':
                config.show_help = true;
                break;
                
            case '?':
                return std::unexpected(ParseError::UnknownOption);
                
            default:
                return std::unexpected(ParseError::InvalidArgument);
        }
    }
    
    // Check for unexpected non-option arguments
    if (optind < argc) {
        return std::unexpected(ParseError::InvalidArgument);
    }
    
    return config;
}

//===============================================================================
// HELP AND ERROR HANDLING
//===============================================================================

void print_help_modern(std::string_view program_name) {
    using namespace std::string_view_literals;
    
    std::cout << std::format("\n{}NAME{}\n", COLOR_BRIGHT_MAGENTA, COLOR_RESET);
    std::cout << std::format("  {} - an entertaining and engaging five-in-a-row version\n\n", program_name);

    std::cout << std::format("{}FLAGS:{}\n", COLOR_BRIGHT_MAGENTA, COLOR_RESET);
    std::cout << std::format("  {}-d, --depth N{}         The depth of search in the MiniMax algorithm\n", 
                            COLOR_YELLOW, COLOR_RESET);
    std::cout << std::format("  {}-l, --level M{}         Can be \"easy\", \"intermediate\", \"hard\"\n", 
                            COLOR_YELLOW, COLOR_RESET);
    std::cout << std::format("  {}-t, --timeout T{}       Timeout in seconds for moves\n", 
                            COLOR_YELLOW, COLOR_RESET);
    std::cout << std::format("  {}-b, --board 15,19{}     Board size. Can be either 19 or 15.\n", 
                            COLOR_YELLOW, COLOR_RESET);
    std::cout << std::format("  {}-u, --undo{}            Enable the Undo feature\n", 
                            COLOR_YELLOW, COLOR_RESET);
    std::cout << std::format("  {}-s, --skip-welcome{}    Skip the welcome screen\n", 
                            COLOR_YELLOW, COLOR_RESET);
    std::cout << std::format("  {}-h, --help{}            Show this help message\n", 
                            COLOR_YELLOW, COLOR_RESET);

    std::cout << std::format("\n{}EXAMPLES:{}\n", COLOR_BRIGHT_MAGENTA, COLOR_RESET);
    std::cout << std::format("  {}{} --level easy --board 15{}\n", 
                            COLOR_YELLOW, program_name, COLOR_RESET);
    std::cout << std::format("  {}{} -d 4 -t 30 -b 19{}\n", 
                            COLOR_YELLOW, program_name, COLOR_RESET);
    std::cout << std::format("  {}{} --level hard --timeout 60{}\n", 
                            COLOR_YELLOW, program_name, COLOR_RESET);

    std::cout << std::format("\n{}DIFFICULTY LEVELS:{}\n", COLOR_BRIGHT_MAGENTA, COLOR_RESET);
    std::cout << std::format("  {}easy{}         - Search depth 2 (quick moves, good for beginners)\n", 
                            COLOR_GREEN, COLOR_RESET);
    std::cout << std::format("  {}intermediate{} - Search depth 4 (balanced gameplay, default setting)\n", 
                            COLOR_GREEN, COLOR_RESET);
    std::cout << std::format("  {}hard{}         - Search depth 6 (advanced AI, challenging for experts)\n", 
                            COLOR_GREEN, COLOR_RESET);

    std::cout << std::format("\n{}DEVELOPER INFO:{}\n", COLOR_BRIGHT_MAGENTA, COLOR_RESET);
    std::cout << std::format("  {}{}{}\n", COLOR_BRIGHT_GREEN, gomoku::GAME_COPYRIGHT, COLOR_RESET);
    std::cout << std::format("  {}Version {}{} | Source: {}{}{}\n", 
                            COLOR_BRIGHT_MAGENTA, gomoku::GAME_VERSION, COLOR_RESET,
                            COLOR_BRIGHT_MAGENTA, gomoku::GAME_URL, COLOR_RESET);
    std::cout << '\n';
}

std::string_view error_to_string(ParseError error) {
    using namespace std::string_view_literals;
    
    switch (error) {
        case ParseError::InvalidArgument:
            return "Invalid argument provided"sv;
        case ParseError::InvalidBoardSize:
            return "Board size must be either 15 or 19"sv;
        case ParseError::InvalidDepth:
            return "Search depth must be between 1 and 10"sv;
        case ParseError::InvalidTimeout:
            return "Timeout must be a positive number"sv;
        case ParseError::UnknownOption:
            return "Unknown option or missing argument"sv;
        case ParseError::MissingValue:
            return "Missing required value for option"sv;
    }
    return "Unknown error"sv;
}

} // namespace gomoku::cli

//===============================================================================
// C-COMPATIBLE INTERFACE (for gradual migration)
//===============================================================================

extern "C" {

cli_config_t parse_arguments(int argc, char* argv[]) {
    // Create span from argc/argv
    std::span<const char*> args{const_cast<const char**>(argv), static_cast<size_t>(argc)};
    
    auto result = gomoku::cli::parse_arguments_modern(args);
    
    if (result.has_value()) {
        return result->to_c_struct();
    } else {
        // Handle error case
        std::cerr << "Error: " << gomoku::cli::error_to_string(result.error()) << '\n';
        cli_config_t config{};
        config.invalid_args = 1;
        return config;
    }
}

void print_help(const char* program_name) {
    gomoku::cli::print_help_modern(std::string_view{program_name});
}

int validate_config(const cli_config_t* config) {
    return !config->invalid_args;
}

}