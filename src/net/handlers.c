//
//  handlers.c
//  gomoku-httpd - HTTP endpoint handlers
//

#define HTTPSERVER_IMPL
#include "handlers.h"
#include "ai.h"
#include "board.h"
#include "game.h"
#include "gomoku.h"
#include "httpserver.h"
#include "json_api.h"
#include "logger.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

//===============================================================================
// GLOBAL STATE
//===============================================================================

static time_t daemon_start_time = 0;

// Atomic flag for busy status (used by HAProxy agent-check)
// Using sig_atomic_t for signal-safe access from agent thread
static volatile sig_atomic_t server_busy = 0;

//===============================================================================
// REQUEST CONTEXT FOR LOGGING
//===============================================================================

typedef struct {
  double start_time;
  char client_ip[INET_ADDRSTRLEN];
  const char *path;
} request_context_t;

static __thread request_context_t current_request = {0};

/**
 * Get client IP address from request socket.
 */
static void get_client_ip(struct http_request_s *request, char *ip_buf,
                          size_t buf_len) {
  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);

  if (getpeername(request->socket, (struct sockaddr *)&addr, &addr_len) == 0) {
    inet_ntop(AF_INET, &addr.sin_addr, ip_buf, (socklen_t)buf_len);
  } else {
    strncpy(ip_buf, "unknown", buf_len);
    ip_buf[buf_len - 1] = '\0';
  }
}

//===============================================================================
// INTERNAL HELPERS
//===============================================================================

/**
 * Send JSON response with specified status code and log the request.
 */
static void send_json_response(struct http_request_s *request, int status,
                               const char *json_body) {
  struct http_response_s *response = http_response_init();
  http_response_status(response, status);
  http_response_header(response, "Content-Type", "application/json");
  http_response_header(response, "Access-Control-Allow-Origin", "*");

  if (json_body) {
    http_response_body(response, json_body, (int)strlen(json_body));
  }

  http_respond(request, response);

  // Log request completion
  double elapsed_ms =
      (get_current_time() - current_request.start_time) * 1000.0;
  LOG_INFO("%s %s %d %.3fms", current_request.client_ip,
           current_request.path ? current_request.path : "/unknown", status,
           elapsed_ms);
}

/**
 * Check if HTTP method matches expected method.
 */
static int method_matches(struct http_request_s *request, const char *method) {
  struct http_string_s req_method = http_request_method(request);
  size_t method_len = strlen(method);

  if ((size_t)req_method.len != method_len) {
    return 0;
  }

  return strncmp(req_method.buf, method, method_len) == 0;
}

/**
 * Check if path matches expected path.
 */
static int path_matches(struct http_request_s *request, const char *path) {
  struct http_string_s target = http_request_target(request);
  size_t path_len = strlen(path);

  // Handle exact match or query string after path
  if ((size_t)target.len < path_len) {
    return 0;
  }

  if (strncmp(target.buf, path, path_len) != 0) {
    return 0;
  }

  // Must be exact match or followed by '?' for query string
  if ((size_t)target.len == path_len || target.buf[path_len] == '?') {
    return 1;
  }

  return 0;
}

//===============================================================================
// PUBLIC FUNCTIONS - INITIALIZATION
//===============================================================================

void handlers_init(void) {
  // Initialize threat matrix for AI
  populate_threat_matrix();

  // Record start time for uptime calculation
  daemon_start_time = time(NULL);

  // Initialize busy flag
  server_busy = 0;

  LOG_INFO("Handlers initialized");
}

//===============================================================================
// PUBLIC FUNCTIONS - BUSY STATUS
//===============================================================================

int handlers_is_busy(void) { return server_busy != 0; }

void handlers_set_busy(void) { server_busy = 1; }

void handlers_set_ready(void) { server_busy = 0; }

//===============================================================================
// PUBLIC FUNCTIONS - REQUEST DISPATCHER
//===============================================================================

void handle_request(struct http_request_s *request) {
  // Initialize request context for logging
  current_request.start_time = get_current_time();
  get_client_ip(request, current_request.client_ip,
                sizeof(current_request.client_ip));

  struct http_string_s method = http_request_method(request);
  struct http_string_s target = http_request_target(request);

  // Store path for logging (null-terminate for safety)
  static __thread char path_buf[256];
  size_t path_len = (size_t)target.len < sizeof(path_buf) - 1
                        ? (size_t)target.len
                        : sizeof(path_buf) - 1;
  memcpy(path_buf, target.buf, path_len);
  path_buf[path_len] = '\0';
  current_request.path = path_buf;

  LOG_DEBUG("Request: %.*s %.*s from %s", (int)method.len, method.buf,
            (int)target.len, target.buf, current_request.client_ip);

  // Handle CORS preflight
  if (method_matches(request, "OPTIONS")) {
    struct http_response_s *response = http_response_init();
    http_response_status(response, 204);
    http_response_header(response, "Access-Control-Allow-Origin", "*");
    http_response_header(response, "Access-Control-Allow-Methods",
                         "GET, POST, OPTIONS");
    http_response_header(response, "Access-Control-Allow-Headers",
                         "Content-Type");
    http_response_header(response, "Access-Control-Max-Age", "86400");
    http_respond(request, response);
    // Log CORS preflight
    double elapsed_ms =
        (get_current_time() - current_request.start_time) * 1000.0;
    LOG_INFO("%s %s 204 %.3fms", current_request.client_ip, path_buf,
             elapsed_ms);
    return;
  }

  // Route to appropriate handler
  if (path_matches(request, "/health")) {
    if (method_matches(request, "GET")) {
      handle_health(request);
    } else {
      handle_method_not_allowed(request);
    }
  } else if (path_matches(request, "/ready")) {
    if (method_matches(request, "GET")) {
      handle_ready(request);
    } else {
      handle_method_not_allowed(request);
    }
  } else if (path_matches(request, "/gomoku/play")) {
    if (method_matches(request, "POST")) {
      handle_play(request);
    } else {
      handle_method_not_allowed(request);
    }
  } else {
    handle_not_found(request);
  }
}

//===============================================================================
// PUBLIC FUNCTIONS - ENDPOINT HANDLERS
//===============================================================================

void handle_health(struct http_request_s *request) {
  char *response_json = json_api_health_response(daemon_start_time);

  if (response_json) {
    LOG_DEBUG("Health check OK");
    send_json_response(request, 200, response_json);
    free(response_json);
  } else {
    handle_internal_error(request, "Failed to generate health response");
  }
}

void handle_ready(struct http_request_s *request) {
  // Readiness check for Envoy/load balancers
  // Returns 200 when server is idle and ready to accept requests
  // Returns 503 when server is busy processing a request
  //
  // This is different from /health which always returns 200 if server is alive.
  // Load balancers can use /ready to route requests to idle servers.
  if (handlers_is_busy()) {
    LOG_DEBUG("Readiness check: BUSY (503)");
    send_json_response(request, 503, "{\"status\":\"busy\"}");
  } else {
    LOG_DEBUG("Readiness check: READY (200)");
    send_json_response(request, 200, "{\"status\":\"ready\"}");
  }
}

void handle_play(struct http_request_s *request) {
  // Get request body
  struct http_string_s body = http_request_body(request);

  if (body.len == 0) {
    LOG_WARN("Empty request body");
    handle_bad_request(request, "Request body is required");
    return;
  }

  // Create null-terminated copy of body
  char *body_str = malloc(body.len + 1);
  if (!body_str) {
    handle_internal_error(request, "Memory allocation failed");
    return;
  }
  memcpy(body_str, body.buf, body.len);
  body_str[body.len] = '\0';

  LOG_DEBUG("Received game state: %zu bytes", body.len);

  // Parse game state
  char error_msg[256] = {0};
  game_state_t *game =
      json_api_parse_game(body_str, error_msg, sizeof(error_msg));
  free(body_str);

  if (!game) {
    LOG_WARN("Failed to parse game: %s", error_msg);
    handle_bad_request(request, error_msg);
    return;
  }

  // Check if game already has a winner
  if (json_api_has_winner(game)) {
    LOG_DEBUG("Game already finished, returning unchanged");
    char *response_json = json_api_serialize_game(game);
    cleanup_game(game);

    if (response_json) {
      send_json_response(request, 200, response_json);
      free(response_json);
    } else {
      handle_internal_error(request, "Failed to serialize game state");
    }
    return;
  }

  // Ensure the next player to move is AI
  int ai_player = game->current_player;
  int player_index = (ai_player == AI_CELL_CROSSES) ? 0 : 1;
  if (game->player_type[player_index] != PLAYER_TYPE_AI) {
    cleanup_game(game);
    handle_bad_request(
        request,
        "Next player is human; server only accepts AI to-move positions");
    return;
  }

  // Set player-specific depth for AI search (same pattern as main.c)
  int saved_depth = game->max_depth;
  game->max_depth = game->depth_for_player[player_index];

  LOG_DEBUG("AI playing as %s, move %d (depth=%d, radius=%d)",
            (ai_player == AI_CELL_CROSSES) ? "X" : "O",
            game->move_history_count + 1, game->max_depth, game->search_radius);

  // Mark server as busy for HAProxy agent-check
  handlers_set_busy();

  // Start timing
  double start_time = get_current_time();
  game->search_start_time = start_time;
  game->search_timed_out = 0;

  // Find best move
  int best_x = -1, best_y = -1;
  const char *move_type = "minimax";

  if (game->move_history_count == 0) {
    // First move of game - play center
    best_x = game->board_size / 2;
    best_y = game->board_size / 2;
    move_type = "center";
  } else if (game->move_history_count == 1) {
    // Second move - play adjacent to first
    find_first_ai_move(game, &best_x, &best_y);
    move_type = "adjacent";
  } else {
    // Use minimax
    find_best_ai_move(game, &best_x, &best_y);
  }

  // Mark server as ready after AI computation
  handlers_set_ready();

  // Restore original max_depth
  game->max_depth = saved_depth;

  double elapsed_time = get_current_time() - start_time;

  // Make the move
  if (best_x < 0 || best_y < 0) {
    LOG_ERROR("AI failed to find valid move after %.3fs", elapsed_time);
    cleanup_game(game);
    handle_internal_error(request, "AI failed to find a valid move");
    return;
  }

  // Get threat scores for the specific move position
  // own_score: what threat does this move create for AI?
  // opp_score: what threat would opponent have had by playing here?
  int own_score = evaluate_threat_fast(game->board, best_x, best_y, ai_player,
                                       game->board_size);
  int opp_score = evaluate_threat_fast(game->board, best_x, best_y, -ai_player,
                                       game->board_size);
  int moves_evaluated = game->last_ai_moves_evaluated;

  if (!make_move(game, best_x, best_y, ai_player, elapsed_time, moves_evaluated,
                 own_score, opp_score)) {
    LOG_ERROR("Failed to make move at [%d, %d]", best_x, best_y);
    cleanup_game(game);
    handle_internal_error(request, "Failed to apply AI move");
    return;
  }

  LOG_DEBUG("AI move [%d,%d] via %s: %.3fs, %d evals, score=%d, opp=%d", best_x,
            best_y, move_type, elapsed_time, moves_evaluated, own_score,
            opp_score);

  // Check for winner after move
  check_game_state(game);

  if (json_api_has_winner(game)) {
    // Mark the last move as winning
    if (game->move_history_count > 0) {
      game->move_history[game->move_history_count - 1].is_winner = 1;
    }

    const char *winner = "draw";
    if (game->game_state == GAME_HUMAN_WIN) {
      winner = "X";
    } else if (game->game_state == GAME_AI_WIN) {
      winner = "O";
    }
    LOG_INFO("Game over: %s wins after %d moves", winner,
             game->move_history_count);
  }

  // Log move details at INFO level
  int player_depth = game->depth_for_player[player_index];
  LOG_INFO("Move %d: %s [%d,%d] depth=%d radius=%d evals=%d time=%.3fs",
           game->move_history_count, (ai_player == AI_CELL_CROSSES) ? "X" : "O",
           best_x, best_y, player_depth, game->search_radius, moves_evaluated,
           elapsed_time);

  // Serialize and return
  char *response_json = json_api_serialize_game(game);
  cleanup_game(game);

  if (response_json) {
    send_json_response(request, 200, response_json);
    free(response_json);
  } else {
    handle_internal_error(request, "Failed to serialize game state");
  }
}

void handle_not_found(struct http_request_s *request) {
  LOG_DEBUG("Not found");
  char *error_json = json_api_error_response("Not found");
  if (error_json) {
    send_json_response(request, 404, error_json);
    free(error_json);
  } else {
    send_json_response(request, 404, "{\"error\":\"Not found\"}");
  }
}

void handle_method_not_allowed(struct http_request_s *request) {
  LOG_DEBUG("Method not allowed");
  char *error_json = json_api_error_response("Method not allowed");
  if (error_json) {
    send_json_response(request, 405, error_json);
    free(error_json);
  } else {
    send_json_response(request, 405, "{\"error\":\"Method not allowed\"}");
  }
}

void handle_bad_request(struct http_request_s *request,
                        const char *error_message) {
  LOG_WARN("Bad request: %s", error_message);
  char *error_json = json_api_error_response(error_message);
  if (error_json) {
    send_json_response(request, 400, error_json);
    free(error_json);
  } else {
    send_json_response(request, 400, "{\"error\":\"Bad request\"}");
  }
}

void handle_internal_error(struct http_request_s *request,
                           const char *error_message) {
  LOG_ERROR("Internal error: %s", error_message);
  char *error_json = json_api_error_response(error_message);
  if (error_json) {
    send_json_response(request, 500, error_json);
    free(error_json);
  } else {
    send_json_response(request, 500, "{\"error\":\"Internal server error\"}");
  }
}
