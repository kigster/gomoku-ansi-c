//
//  json_api.c
//  gomoku-httpd - JSON parsing and serialization for HTTP API
//

#include "json_api.h"
#include "game.h"
#include "gomoku.h"
#include "json.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//===============================================================================
// INTERNAL HELPERS
//===============================================================================

/**
 * Helper to create a JSON number string with exactly 3 decimal places for
 * milliseconds
 */
static json_object *json_ms_from_seconds(double seconds) {
  char buf[32];
  double ms = round(seconds * 1000000.0) / 1000.0; // Round to microseconds
  snprintf(buf, sizeof(buf), "%.3f", ms);
  return json_object_new_double_s(atof(buf), buf);
}

/**
 * Parse player configuration from JSON object.
 * Returns 1 on success, 0 on failure.
 */
static int parse_player_config(json_object *player_obj, player_type_t *type,
                               int *depth, char *error_msg, size_t error_len) {
  if (!player_obj) {
    *type = PLAYER_TYPE_HUMAN;
    *depth = -1;
    return 1;
  }

  json_object *player_type_obj;
  if (json_object_object_get_ex(player_obj, "player", &player_type_obj)) {
    const char *player_str = json_object_get_string(player_type_obj);
    if (player_str) {
      if (strcasecmp(player_str, "AI") == 0) {
        *type = PLAYER_TYPE_AI;
      } else if (strcasecmp(player_str, "human") == 0) {
        *type = PLAYER_TYPE_HUMAN;
      } else {
        snprintf(error_msg, error_len,
                 "Invalid player type: expected 'human' or 'AI'");
        return 0;
      }
    }
  } else {
    *type = PLAYER_TYPE_HUMAN;
  }

  json_object *depth_obj;
  if (json_object_object_get_ex(player_obj, "depth", &depth_obj)) {
    *depth = json_object_get_int(depth_obj);
  } else {
    *depth = -1;
  }

  return 1;
}

//===============================================================================
// PUBLIC FUNCTIONS - PARSING
//===============================================================================

game_state_t *json_api_parse_game(const char *json_str, char *error_msg,
                                  size_t error_msg_len) {
  if (!json_str || !error_msg) {
    if (error_msg && error_msg_len > 0) {
      snprintf(error_msg, error_msg_len, "Invalid parameters");
    }
    return NULL;
  }

  error_msg[0] = '\0';

  // Parse JSON string
  json_object *root = json_tokener_parse(json_str);
  if (!root) {
    snprintf(error_msg, error_msg_len, "Invalid JSON syntax");
    return NULL;
  }

  // Parse board size (optional, defaults to 19)
  json_object *board_obj;
  int board_size = 19;
  if (json_object_object_get_ex(root, "board_size", &board_obj)) {
    board_size = json_object_get_int(board_obj);
    if (board_size != 15 && board_size != 19) {
      snprintf(error_msg, error_msg_len,
               "Invalid board size: must be 15 or 19");
      json_object_put(root);
      return NULL;
    }
  }

  // Parse player configurations (required)
  player_type_t player_x_type = PLAYER_TYPE_HUMAN;
  player_type_t player_o_type = PLAYER_TYPE_AI;
  int depth_x = -1;
  int depth_o = -1;

  json_object *x_obj, *o_obj;
  if (!json_object_object_get_ex(root, "X", &x_obj)) {
    snprintf(error_msg, error_msg_len, "Missing required field: X");
    json_object_put(root);
    return NULL;
  }
  if (!json_object_object_get_ex(root, "O", &o_obj)) {
    snprintf(error_msg, error_msg_len, "Missing required field: O");
    json_object_put(root);
    return NULL;
  }

  json_object *player_type_obj;
  if (!json_object_object_get_ex(x_obj, "player", &player_type_obj)) {
    snprintf(error_msg, error_msg_len, "Missing required field: X.player");
    json_object_put(root);
    return NULL;
  }
  if (!json_object_object_get_ex(o_obj, "player", &player_type_obj)) {
    snprintf(error_msg, error_msg_len, "Missing required field: O.player");
    json_object_put(root);
    return NULL;
  }

  if (!parse_player_config(x_obj, &player_x_type, &depth_x, error_msg,
                           error_msg_len)) {
    json_object_put(root);
    return NULL;
  }
  if (!parse_player_config(o_obj, &player_o_type, &depth_o, error_msg,
                           error_msg_len)) {
    json_object_put(root);
    return NULL;
  }

  // Parse radius (cap to API_MAX_RADIUS)
  int radius = 2;
  json_object *radius_obj;
  if (json_object_object_get_ex(root, "radius", &radius_obj)) {
    radius = json_object_get_int(radius_obj);
    if (radius > API_MAX_RADIUS) {
      radius = API_MAX_RADIUS;
    }
    if (radius < 1) {
      radius = 1;
    }
  }

  // Cap depths to API_MAX_DEPTH
  if (depth_x > API_MAX_DEPTH) {
    depth_x = API_MAX_DEPTH;
  }
  if (depth_o > API_MAX_DEPTH) {
    depth_o = API_MAX_DEPTH;
  }

  // Parse timeout
  int timeout = 0;
  json_object *timeout_obj;
  if (json_object_object_get_ex(root, "timeout", &timeout_obj)) {
    if (json_object_is_type(timeout_obj, json_type_int)) {
      timeout = json_object_get_int(timeout_obj);
    }
    // "none" string results in timeout = 0
  }

  // Create cli_config for game initialization
  cli_config_t config = {
      .board_size = board_size,
      .max_depth = API_MAX_DEPTH,
      .move_timeout = timeout,
      .show_help = 0,
      .invalid_args = 0,
      .enable_undo = 0,
      .skip_welcome = 1,
      .headless = 1, // Daemon mode - no stdout output
      .search_radius = radius,
      .json_file = "",
      .replay_file = "",
      .replay_wait = 0,
      .player_x_type = player_x_type,
      .player_o_type = player_o_type,
      .depth_x = (depth_x > 0) ? depth_x : API_MAX_DEPTH,
      .depth_o = (depth_o > 0) ? depth_o : API_MAX_DEPTH,
      .player_x_explicit = 1,
      .player_o_explicit = 1,
  };

  // Initialize game state
  game_state_t *game = init_game(config);
  if (!game) {
    snprintf(error_msg, error_msg_len, "Failed to initialize game state");
    json_object_put(root);
    return NULL;
  }

  // Parse and replay moves
  json_object *moves_obj;
  if (json_object_object_get_ex(root, "moves", &moves_obj)) {
    int num_moves = json_object_array_length(moves_obj);

    for (int i = 0; i < num_moves; i++) {
      json_object *move_obj = json_object_array_get_idx(moves_obj, i);
      if (!move_obj)
        continue;

      int x = -1, y = -1;
      int player = 0;
      double time_taken = 0;
      int positions_evaluated = 0;
      int own_score = 0;
      int opponent_score = 0;

      // Parse move object
      json_object_object_foreach(move_obj, key, val) {
        // Check for position array (the player key)
        if (json_object_is_type(val, json_type_array) &&
            json_object_array_length(val) == 2) {
          x = json_object_get_int(json_object_array_get_idx(val, 0));
          y = json_object_get_int(json_object_array_get_idx(val, 1));

          // Determine player from key
          if (key[0] == 'X') {
            player = AI_CELL_CROSSES;
          } else if (key[0] == 'O') {
            player = AI_CELL_NAUGHTS;
          }
        } else if (strcmp(key, "time_ms") == 0) {
          time_taken = json_object_get_double(val) / 1000.0;
        } else if (strcmp(key, "moves_evaluated") == 0 ||
                   strcmp(key, "moves_searched") == 0) {
          // Accept both new and old field names for backwards compatibility
          positions_evaluated = json_object_get_int(val);
        } else if (strcmp(key, "score") == 0) {
          own_score = json_object_get_int(val);
        } else if (strcmp(key, "opponent") == 0) {
          opponent_score = json_object_get_int(val);
        }
      }

      // Validate and make move
      if (x >= 0 && y >= 0 && player != 0) {
        if (!make_move(game, x, y, player, time_taken, positions_evaluated,
                       own_score, opponent_score)) {
          snprintf(error_msg, error_msg_len,
                   "Invalid move at position [%d, %d]", x, y);
          cleanup_game(game);
          json_object_put(root);
          return NULL;
        }
      }
    }
  }

  // Check game state after replaying moves
  check_game_state(game);

  // Set current player to opposite of last move
  if (game->move_history_count > 0) {
    int last_player = game->move_history[game->move_history_count - 1].player;
    game->current_player =
        (last_player == AI_CELL_CROSSES) ? AI_CELL_NAUGHTS : AI_CELL_CROSSES;
  }

  json_object_put(root);
  return game;
}

//===============================================================================
// PUBLIC FUNCTIONS - SERIALIZATION
//===============================================================================

char *json_api_serialize_game(game_state_t *game) {
  if (!game) {
    return NULL;
  }

  json_object *root = json_object_new_object();
  if (!root) {
    return NULL;
  }

  // Player X configuration
  json_object *player_x = json_object_new_object();
  json_object_object_add(
      player_x, "player",
      json_object_new_string(game->player_type[0] == PLAYER_TYPE_HUMAN ? "human"
                                                                       : "AI"));
  if (game->player_type[0] == PLAYER_TYPE_AI) {
    json_object_object_add(player_x, "depth",
                           json_object_new_int(game->depth_for_player[0]));
  }
  json_object_object_add(player_x, "time_ms",
                         json_ms_from_seconds(game->total_human_time));
  json_object_object_add(root, "X", player_x);

  // Player O configuration
  json_object *player_o = json_object_new_object();
  json_object_object_add(
      player_o, "player",
      json_object_new_string(game->player_type[1] == PLAYER_TYPE_HUMAN ? "human"
                                                                       : "AI"));
  if (game->player_type[1] == PLAYER_TYPE_AI) {
    json_object_object_add(player_o, "depth",
                           json_object_new_int(game->depth_for_player[1]));
  }
  json_object_object_add(player_o, "time_ms",
                         json_ms_from_seconds(game->total_ai_time));
  json_object_object_add(root, "O", player_o);

  // Game parameters
  json_object_object_add(root, "board_size",
                         json_object_new_int(game->board_size));
  json_object_object_add(root, "radius",
                         json_object_new_int(game->search_radius));

  if (game->move_timeout > 0) {
    json_object_object_add(root, "timeout",
                           json_object_new_int(game->move_timeout));
  } else {
    json_object_object_add(root, "timeout", json_object_new_string("none"));
  }

  // Winner
  const char *winner_str = "none";
  if (game->game_state == GAME_HUMAN_WIN) {
    winner_str = "X";
  } else if (game->game_state == GAME_AI_WIN) {
    winner_str = "O";
  } else if (game->game_state == GAME_DRAW) {
    winner_str = "draw";
  }
  json_object_object_add(root, "winner", json_object_new_string(winner_str));

  // Board state as array of row strings
  json_object *board_array = json_object_new_array();
  const char *x_symbol = "X"; // Use ASCII for API
  const char *o_symbol = "O";
  int board_size = game->board_size;

  for (int row = 0; row < board_size; row++) {
    // Allocate buffer for row string (symbols + spaces)
    size_t row_len = (size_t)(board_size * 2);
    char *row_str = malloc(row_len + 1);
    if (row_str) {
      int idx = 0;
      for (int col = 0; col < board_size; col++) {
        int cell = game->board[row][col];
        if (cell == AI_CELL_CROSSES) {
          row_str[idx++] = x_symbol[0];
        } else if (cell == AI_CELL_NAUGHTS) {
          row_str[idx++] = o_symbol[0];
        } else {
          row_str[idx++] = '.';
        }
        if (col < board_size - 1) {
          row_str[idx++] = ' ';
        }
      }
      row_str[idx] = '\0';
      json_object_array_add(board_array, json_object_new_string(row_str));
      free(row_str);
    }
  }
  json_object_object_add(root, "board_state", board_array);

  // Moves array
  json_object *moves_array = json_object_new_array();

  for (int i = 0; i < game->move_history_count; i++) {
    move_history_t *move = &game->move_history[i];
    json_object *move_obj = json_object_new_object();

    // Player identifier
    const char *player_name;
    int player_index = (move->player == AI_CELL_CROSSES) ? 0 : 1;
    int is_ai = (game->player_type[player_index] == PLAYER_TYPE_AI);

    if (move->player == AI_CELL_CROSSES) {
      player_name = is_ai ? "X (AI)" : "X (human)";
    } else {
      player_name = is_ai ? "O (AI)" : "O (human)";
    }

    // Position array [x, y]
    json_object *pos_array = json_object_new_array();
    json_object_array_add(pos_array, json_object_new_int(move->x));
    json_object_array_add(pos_array, json_object_new_int(move->y));
    json_object_object_add(move_obj, player_name, pos_array);

    // AI-specific fields
    if (is_ai && move->positions_evaluated > 0) {
      json_object_object_add(move_obj, "moves_evaluated",
                             json_object_new_int(move->positions_evaluated));
    }
    if (is_ai && move->own_score != 0) {
      json_object_object_add(move_obj, "score",
                             json_object_new_int(move->own_score));
    }
    if (is_ai && move->opponent_score != 0) {
      json_object_object_add(move_obj, "opponent",
                             json_object_new_int(move->opponent_score));
    }

    // Time taken
    json_object_object_add(move_obj, "time_ms",
                           json_ms_from_seconds(move->time_taken));

    // Winner flag
    if (move->is_winner) {
      json_object_object_add(move_obj, "winner", json_object_new_boolean(1));
    }

    json_object_array_add(moves_array, move_obj);
  }

  json_object_object_add(root, "moves", moves_array);

  // Convert to string
  const char *json_str =
      json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);

  char *result = NULL;
  if (json_str) {
    result = strdup(json_str);
  }

  json_object_put(root);
  return result;
}

//===============================================================================
// PUBLIC FUNCTIONS - RESPONSE HELPERS
//===============================================================================

char *json_api_error_response(const char *error_message) {
  json_object *root = json_object_new_object();
  if (!root) {
    return NULL;
  }

  json_object_object_add(
      root, "error",
      json_object_new_string(error_message ? error_message : "Unknown error"));

  const char *json_str =
      json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);

  char *result = NULL;
  if (json_str) {
    result = strdup(json_str);
  }

  json_object_put(root);
  return result;
}

char *json_api_health_response(time_t start_time) {
  json_object *root = json_object_new_object();
  if (!root) {
    return NULL;
  }

  json_object_object_add(root, "status", json_object_new_string("ok"));
  json_object_object_add(root, "version", json_object_new_string(API_VERSION));

  // Calculate uptime
  time_t now = time(NULL);
  long uptime_secs = (long)(now - start_time);

  char uptime_str[64];
  json_api_format_uptime(uptime_secs, uptime_str, sizeof(uptime_str));
  json_object_object_add(root, "uptime", json_object_new_string(uptime_str));

  const char *json_str =
      json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);

  char *result = NULL;
  if (json_str) {
    result = strdup(json_str);
  }

  json_object_put(root);
  return result;
}

//===============================================================================
// PUBLIC FUNCTIONS - UTILITY
//===============================================================================

int json_api_determine_ai_player(game_state_t *game) {
  if (!game || game->move_history_count == 0) {
    // No moves yet - AI plays as O (second player)
    return AI_CELL_NAUGHTS;
  }

  // Return opposite of last move's player
  int last_player = game->move_history[game->move_history_count - 1].player;
  return (last_player == AI_CELL_CROSSES) ? AI_CELL_NAUGHTS : AI_CELL_CROSSES;
}

int json_api_has_winner(game_state_t *game) {
  if (!game) {
    return 0;
  }

  return (game->game_state == GAME_HUMAN_WIN ||
          game->game_state == GAME_AI_WIN || game->game_state == GAME_DRAW);
}

void json_api_format_uptime(long seconds, char *buffer, size_t buffer_len) {
  if (!buffer || buffer_len == 0) {
    return;
  }

  long days = seconds / 86400;
  long hours = (seconds % 86400) / 3600;
  long minutes = (seconds % 3600) / 60;
  long secs = seconds % 60;

  if (days > 0) {
    snprintf(buffer, buffer_len, "%ldd %ldh %ldm %lds", days, hours, minutes,
             secs);
  } else if (hours > 0) {
    snprintf(buffer, buffer_len, "%ldh %ldm %lds", hours, minutes, secs);
  } else if (minutes > 0) {
    snprintf(buffer, buffer_len, "%ldm %lds", minutes, secs);
  } else {
    snprintf(buffer, buffer_len, "%lds", secs);
  }
}
