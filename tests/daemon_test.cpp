#include <cstdlib>
#include <cstring>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

// Include the C headers in C++ context
extern "C" {
#include "game.h"
#include "gomoku.h"
#include "net/cli.h"
#include "net/json_api.h"
#include "net/test_client_utils.h"
}

// Helper function to read fixture file
static std::string read_fixture(const char *filename) {
  std::string path = "tests/fixtures/";
  path += filename;

  std::ifstream file(path);
  if (!file.is_open()) {
    // Try from current directory
    path = filename;
    file.open(path);
  }
  if (!file.is_open()) {
    return "";
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

//===============================================================================
// JSON API TESTS
//===============================================================================

class DaemonJsonTest : public ::testing::Test {
protected:
  void SetUp() override { populate_threat_matrix(); }
};

// Test parsing valid game at start
TEST_F(DaemonJsonTest, ParseValidGameStart) {
  std::string json = read_fixture("valid_game_start.json");
  ASSERT_FALSE(json.empty()) << "Could not read fixture file";

  char error[256] = {0};
  game_state_t *game = json_api_parse_game(json.c_str(), error, sizeof(error));

  ASSERT_NE(game, nullptr) << "Parse failed: " << error;
  EXPECT_EQ(game->board_size, 19);
  EXPECT_EQ(game->move_history_count, 1);
  EXPECT_EQ(game->search_radius, 2);

  // Check the move was parsed correctly
  EXPECT_EQ(game->move_history[0].x, 9);
  EXPECT_EQ(game->move_history[0].y, 9);
  EXPECT_EQ(game->move_history[0].player, AI_CELL_CROSSES);

  // Current player should be O (opposite of last move)
  EXPECT_EQ(game->current_player, AI_CELL_NAUGHTS);

  cleanup_game(game);
}

//===============================================================================
// TEST CLIENT UTIL TESTS
//===============================================================================

TEST_F(DaemonJsonTest, TestClientInitialGameStateSetsBothAI) {
  char *json = test_client_create_initial_game_state(15, 3, 2);
  ASSERT_NE(json, nullptr);

  char error[256] = {0};
  game_state_t *game = json_api_parse_game(json, error, sizeof(error));
  ASSERT_NE(game, nullptr) << "Parse failed: " << error;

  EXPECT_EQ(game->board_size, 15);
  EXPECT_EQ(game->search_radius, 2);
  EXPECT_EQ(game->move_history_count, 0);
  EXPECT_EQ(game->player_type[0], PLAYER_TYPE_AI);
  EXPECT_EQ(game->player_type[1], PLAYER_TYPE_AI);
  EXPECT_EQ(game->depth_for_player[0], 3);
  EXPECT_EQ(game->depth_for_player[1], 3);
  EXPECT_EQ(game->current_player, AI_CELL_CROSSES);

  cleanup_game(game);
  free(json);
}

TEST_F(DaemonJsonTest, TestClientParsesLastMove) {
  const char *json =
      "{\n"
      "  \"X\": { \"player\": \"AI\", \"depth\": 2, \"time_ms\": 0.000 },\n"
      "  \"O\": { \"player\": \"AI\", \"depth\": 2, \"time_ms\": 0.000 },\n"
      "  \"board\": 15,\n"
      "  \"radius\": 2,\n"
      "  \"timeout\": \"none\",\n"
      "  \"winner\": \"none\",\n"
      "  \"board_state\": [],\n"
      "  \"moves\": [\n"
      "    { \"X (AI)\": [7, 7], \"time_ms\": 0.000 },\n"
      "    { \"O (AI)\": [7, 8], \"time_ms\": 0.000 },\n"
      "    { \"X (AI)\": [8, 8], \"time_ms\": 0.000 }\n"
      "  ]\n"
      "}\n";

  const char *label = nullptr;
  int x = -1;
  int y = -1;
  EXPECT_EQ(test_client_get_last_move(json, &label, &x, &y), 1);
  EXPECT_STREQ(label, "X (AI)");
  EXPECT_EQ(x, 8);
  EXPECT_EQ(y, 8);
}

// Test parsing valid game in mid-game state
TEST_F(DaemonJsonTest, ParseValidGameMidgame) {
  std::string json = read_fixture("valid_game_midgame.json");
  ASSERT_FALSE(json.empty()) << "Could not read fixture file";

  char error[256] = {0};
  game_state_t *game = json_api_parse_game(json.c_str(), error, sizeof(error));

  ASSERT_NE(game, nullptr) << "Parse failed: " << error;
  EXPECT_EQ(game->board_size, 19);
  EXPECT_EQ(game->move_history_count, 8);

  // Game should not have a winner yet
  EXPECT_EQ(game->game_state, GAME_RUNNING);

  cleanup_game(game);
}

// Test parsing game with existing winner
TEST_F(DaemonJsonTest, ParseValidGameWon) {
  std::string json = read_fixture("valid_game_won.json");
  ASSERT_FALSE(json.empty()) << "Could not read fixture file";

  char error[256] = {0};
  game_state_t *game = json_api_parse_game(json.c_str(), error, sizeof(error));

  ASSERT_NE(game, nullptr) << "Parse failed: " << error;
  EXPECT_EQ(game->board_size, 19);

  // Game should detect X as winner
  EXPECT_EQ(game->game_state, GAME_HUMAN_WIN);

  // json_api_has_winner should return true
  EXPECT_TRUE(json_api_has_winner(game));

  cleanup_game(game);
}

// Test that high depth and radius get capped
TEST_F(DaemonJsonTest, CapsDepthAndRadius) {
  std::string json = read_fixture("valid_game_high_depth.json");
  ASSERT_FALSE(json.empty()) << "Could not read fixture file";

  char error[256] = {0};
  game_state_t *game = json_api_parse_game(json.c_str(), error, sizeof(error));

  ASSERT_NE(game, nullptr) << "Parse failed: " << error;

  // Depth should be capped to API_MAX_DEPTH (4)
  EXPECT_LE(game->depth_for_player[1], API_MAX_DEPTH);

  // Radius should be capped to API_MAX_RADIUS (3)
  EXPECT_LE(game->search_radius, API_MAX_RADIUS);

  cleanup_game(game);
}

// Test parsing JSON with missing board_size field - should default to 19
TEST_F(DaemonJsonTest, ParseMissingBoardDefaultsTo19) {
  std::string json = read_fixture("invalid_missing_board.json");
  ASSERT_FALSE(json.empty()) << "Could not read fixture file";

  char error[256] = {0};
  game_state_t *game = json_api_parse_game(json.c_str(), error, sizeof(error));

  // board_size is now optional and defaults to 19
  ASSERT_NE(game, nullptr) << "Parse should succeed with default board_size";
  EXPECT_EQ(game->board_size, 19) << "Missing board_size should default to 19";

  cleanup_game(game);
}

// Test parsing invalid JSON - overlapping moves
TEST_F(DaemonJsonTest, ParseInvalidBadMoves) {
  std::string json = read_fixture("invalid_bad_moves.json");
  ASSERT_FALSE(json.empty()) << "Could not read fixture file";

  char error[256] = {0};
  game_state_t *game = json_api_parse_game(json.c_str(), error, sizeof(error));

  EXPECT_EQ(game, nullptr);
  EXPECT_STRNE(error, "");
}

// Test parsing malformed JSON
TEST_F(DaemonJsonTest, ParseMalformedJson) {
  const char *json = "{ invalid json }";

  char error[256] = {0};
  game_state_t *game = json_api_parse_game(json, error, sizeof(error));

  EXPECT_EQ(game, nullptr);
  EXPECT_STRNE(error, "");
}

// Test parsing empty input
TEST_F(DaemonJsonTest, ParseEmptyInput) {
  char error[256] = {0};
  game_state_t *game = json_api_parse_game("", error, sizeof(error));

  EXPECT_EQ(game, nullptr);
}

// Test parsing null input
TEST_F(DaemonJsonTest, ParseNullInput) {
  char error[256] = {0};
  game_state_t *game = json_api_parse_game(nullptr, error, sizeof(error));

  EXPECT_EQ(game, nullptr);
}

//===============================================================================
// SERIALIZATION TESTS
//===============================================================================

// Test serialization round-trip
TEST_F(DaemonJsonTest, SerializeRoundTrip) {
  std::string json = read_fixture("valid_game_start.json");
  ASSERT_FALSE(json.empty()) << "Could not read fixture file";

  char error[256] = {0};
  game_state_t *game = json_api_parse_game(json.c_str(), error, sizeof(error));
  ASSERT_NE(game, nullptr) << "Parse failed: " << error;

  // Serialize
  char *serialized = json_api_serialize_game(game);
  ASSERT_NE(serialized, nullptr);

  // Parse again
  char error2[256] = {0};
  game_state_t *game2 = json_api_parse_game(serialized, error2, sizeof(error2));
  ASSERT_NE(game2, nullptr) << "Re-parse failed: " << error2;

  // Compare key fields
  EXPECT_EQ(game->board_size, game2->board_size);
  EXPECT_EQ(game->move_history_count, game2->move_history_count);
  EXPECT_EQ(game->search_radius, game2->search_radius);

  // Compare moves
  for (int i = 0; i < game->move_history_count; i++) {
    EXPECT_EQ(game->move_history[i].x, game2->move_history[i].x);
    EXPECT_EQ(game->move_history[i].y, game2->move_history[i].y);
    EXPECT_EQ(game->move_history[i].player, game2->move_history[i].player);
  }

  free(serialized);
  cleanup_game(game);
  cleanup_game(game2);
}

// Test serializing null game
TEST_F(DaemonJsonTest, SerializeNullGame) {
  char *result = json_api_serialize_game(nullptr);
  EXPECT_EQ(result, nullptr);
}

//===============================================================================
// RESPONSE HELPER TESTS
//===============================================================================

// Test error response generation
TEST_F(DaemonJsonTest, ErrorResponse) {
  char *response = json_api_error_response("Test error message");
  ASSERT_NE(response, nullptr);

  // Should contain the error message
  EXPECT_NE(strstr(response, "error"), nullptr);
  EXPECT_NE(strstr(response, "Test error message"), nullptr);

  free(response);
}

// Test health response generation
TEST_F(DaemonJsonTest, HealthResponse) {
  time_t start_time = time(nullptr) - 3661; // 1 hour, 1 minute, 1 second ago
  char *response = json_api_health_response(start_time);
  ASSERT_NE(response, nullptr);

  // Should contain expected fields
  EXPECT_NE(strstr(response, "status"), nullptr);
  EXPECT_NE(strstr(response, "ok"), nullptr);
  EXPECT_NE(strstr(response, "version"), nullptr);
  EXPECT_NE(strstr(response, "uptime"), nullptr);

  free(response);
}

//===============================================================================
// UTILITY FUNCTION TESTS
//===============================================================================

// Test AI player determination
TEST_F(DaemonJsonTest, DetermineAiPlayer) {
  std::string json = read_fixture("valid_game_start.json");
  ASSERT_FALSE(json.empty()) << "Could not read fixture file";

  char error[256] = {0};
  game_state_t *game = json_api_parse_game(json.c_str(), error, sizeof(error));
  ASSERT_NE(game, nullptr) << "Parse failed: " << error;

  // After X move, AI should play as O
  int ai_player = json_api_determine_ai_player(game);
  EXPECT_EQ(ai_player, AI_CELL_NAUGHTS);

  cleanup_game(game);
}

// Test AI player determination with no moves
TEST_F(DaemonJsonTest, DetermineAiPlayerNoMoves) {
  // Empty game - AI plays as O (second player)
  int ai_player = json_api_determine_ai_player(nullptr);
  EXPECT_EQ(ai_player, AI_CELL_NAUGHTS);
}

// Test uptime formatting
TEST_F(DaemonJsonTest, FormatUptime) {
  char buffer[64];

  // Test seconds only
  json_api_format_uptime(45, buffer, sizeof(buffer));
  EXPECT_STREQ(buffer, "45s");

  // Test minutes and seconds
  json_api_format_uptime(125, buffer, sizeof(buffer));
  EXPECT_STREQ(buffer, "2m 5s");

  // Test hours, minutes, seconds
  json_api_format_uptime(3661, buffer, sizeof(buffer));
  EXPECT_STREQ(buffer, "1h 1m 1s");

  // Test days
  json_api_format_uptime(90061, buffer, sizeof(buffer));
  EXPECT_STREQ(buffer, "1d 1h 1m 1s");
}

//===============================================================================
// CLI TESTS
//===============================================================================

class DaemonCliTest : public ::testing::Test {
protected:
  FILE *original_stderr;

  void SetUp() override {
    // Suppress stderr during CLI tests (error messages are expected for invalid input tests)
    original_stderr = stderr;
    stderr = fopen("/dev/null", "w");
  }

  void TearDown() override {
    if (stderr != original_stderr) {
      fclose(stderr);
      stderr = original_stderr;
    }
  }
};

// Test parsing bind address with host and port
TEST_F(DaemonCliTest, ParseBindHostPort) {
  const char *argv[] = {"daemon", "-b", "127.0.0.1:3000"};
  daemon_config_t config = daemon_parse_arguments(3, (char **)argv);

  EXPECT_STREQ(config.bind_host, "127.0.0.1");
  EXPECT_EQ(config.bind_port, 3000);
  EXPECT_EQ(config.invalid_args, 0);
}

// Test parsing bind address with port only
TEST_F(DaemonCliTest, ParseBindPortOnly) {
  const char *argv[] = {"daemon", "-b", "8080"};
  daemon_config_t config = daemon_parse_arguments(3, (char **)argv);

  EXPECT_STREQ(config.bind_host, "0.0.0.0");
  EXPECT_EQ(config.bind_port, 8080);
  EXPECT_EQ(config.invalid_args, 0);
}

// Test parsing daemonize flag
TEST_F(DaemonCliTest, ParseDaemonize) {
  const char *argv[] = {"daemon", "-b", "3000", "-d"};
  daemon_config_t config = daemon_parse_arguments(4, (char **)argv);

  EXPECT_EQ(config.daemonize, 1);
  EXPECT_EQ(config.invalid_args, 0);
}

// Test parsing log file
TEST_F(DaemonCliTest, ParseLogFile) {
  const char *argv[] = {"daemon", "-b", "3000", "-l", "/var/log/test.log"};
  daemon_config_t config = daemon_parse_arguments(5, (char **)argv);

  EXPECT_STREQ(config.log_file, "/var/log/test.log");
  EXPECT_EQ(config.invalid_args, 0);
}

// Test parsing log level
TEST_F(DaemonCliTest, ParseLogLevel) {
  const char *argv[] = {"daemon", "-b", "3000", "-L", "DEBUG"};
  daemon_config_t config = daemon_parse_arguments(5, (char **)argv);

  EXPECT_EQ(config.log_level, DAEMON_LOG_DEBUG);
  EXPECT_EQ(config.invalid_args, 0);
}

// Test parsing log level case insensitive
TEST_F(DaemonCliTest, ParseLogLevelCaseInsensitive) {
  const char *argv[] = {"daemon", "-b", "3000", "-L", "warn"};
  daemon_config_t config = daemon_parse_arguments(5, (char **)argv);

  EXPECT_EQ(config.log_level, DAEMON_LOG_WARN);
  EXPECT_EQ(config.invalid_args, 0);
}

// Test parsing help flag
TEST_F(DaemonCliTest, ParseHelp) {
  const char *argv[] = {"daemon", "-h"};
  daemon_config_t config = daemon_parse_arguments(2, (char **)argv);

  EXPECT_EQ(config.show_help, 1);
}

// Test invalid port
TEST_F(DaemonCliTest, InvalidPort) {
  const char *argv[] = {"daemon", "-b", "0"};
  daemon_config_t config = daemon_parse_arguments(3, (char **)argv);

  EXPECT_EQ(config.invalid_args, 1);
}

// Test invalid log level
TEST_F(DaemonCliTest, InvalidLogLevel) {
  const char *argv[] = {"daemon", "-b", "3000", "-L", "INVALID"};
  daemon_config_t config = daemon_parse_arguments(5, (char **)argv);

  EXPECT_EQ(config.invalid_args, 1);
}

// Test config validation - missing bind
TEST_F(DaemonCliTest, ValidateMissingBind) {
  daemon_config_t config = {
      .bind_host = "",
      .bind_port = 0,
      .daemonize = 0,
      .log_file = "",
      .log_level = DAEMON_LOG_INFO,
      .show_help = 0,
      .invalid_args = 0,
  };

  EXPECT_EQ(daemon_validate_config(&config), 0);
}

// Test config validation - valid config
TEST_F(DaemonCliTest, ValidateValidConfig) {
  daemon_config_t config = {
      .bind_host = "127.0.0.1",
      .bind_port = 3000,
      .daemonize = 0,
      .log_file = "",
      .log_level = DAEMON_LOG_INFO,
      .show_help = 0,
      .invalid_args = 0,
  };

  EXPECT_EQ(daemon_validate_config(&config), 1);
}

// Test config validation - help skips validation
TEST_F(DaemonCliTest, ValidateHelpSkipsValidation) {
  daemon_config_t config = {
      .bind_host = "",
      .bind_port = 0,
      .daemonize = 0,
      .log_file = "",
      .log_level = DAEMON_LOG_INFO,
      .show_help = 1,
      .invalid_args = 0,
  };

  EXPECT_EQ(daemon_validate_config(&config), 1);
}

// Test log level parsing
TEST_F(DaemonCliTest, ParseLogLevelFunction) {
  EXPECT_EQ(daemon_parse_log_level("TRACE"), DAEMON_LOG_TRACE);
  EXPECT_EQ(daemon_parse_log_level("DEBUG"), DAEMON_LOG_DEBUG);
  EXPECT_EQ(daemon_parse_log_level("INFO"), DAEMON_LOG_INFO);
  EXPECT_EQ(daemon_parse_log_level("WARN"), DAEMON_LOG_WARN);
  EXPECT_EQ(daemon_parse_log_level("WARNING"), DAEMON_LOG_WARN);
  EXPECT_EQ(daemon_parse_log_level("ERROR"), DAEMON_LOG_ERROR);
  EXPECT_EQ(daemon_parse_log_level("FATAL"), DAEMON_LOG_FATAL);
  EXPECT_EQ((int)daemon_parse_log_level("INVALID"), -1);
  EXPECT_EQ((int)daemon_parse_log_level(nullptr), -1);
}
