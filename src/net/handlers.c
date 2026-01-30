//
//  handlers.c
//  gomoku-http-daemon - HTTP endpoint handlers
//

#define HTTPSERVER_IMPL
#include "handlers.h"
#include "ai.h"
#include "board.h"
#include "game.h"
#include "gomoku.h"
#include "httpserver.h"
#include "json_api.h"
#include "log.h"
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
  log_info("%s %s %d %.3fms", current_request.client_ip,
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

  log_info("Handlers initialized");
}

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

  log_debug("Request: %.*s %.*s from %s", (int)method.len, method.buf,
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
    log_info("%s %s 204 %.3fms", current_request.client_ip, path_buf,
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
    log_debug("Health check OK");
    send_json_response(request, 200, response_json);
    free(response_json);
  } else {
    handle_internal_error(request, "Failed to generate health response");
  }
}

void handle_play(struct http_request_s *request) {
  // Get request body
  struct http_string_s body = http_request_body(request);

  if (body.len == 0) {
    log_warn("Empty request body");
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

  log_debug("Received game state: %zu bytes", body.len);

  // Parse game state
  char error_msg[256] = {0};
  game_state_t *game =
      json_api_parse_game(body_str, error_msg, sizeof(error_msg));
  free(body_str);

  if (!game) {
    log_warn("Failed to parse game: %s", error_msg);
    handle_bad_request(request, error_msg);
    return;
  }

  // Check if game already has a winner
  if (json_api_has_winner(game)) {
    log_debug("Game already finished, returning unchanged");
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

  // Determine which player the AI should play as
  int ai_player = json_api_determine_ai_player(game);
  game->current_player = ai_player;

  log_debug("AI playing as %s, move %d",
            (ai_player == AI_CELL_CROSSES) ? "X" : "O",
            game->move_history_count + 1);

  // Start timing
  double start_time = get_current_time();
  game->search_start_time = start_time;
  game->search_timed_out = 0;

  // Find best move
  int best_x = -1, best_y = -1;

  if (game->move_history_count == 0) {
    // First move of game - play center
    best_x = game->board_size / 2;
    best_y = game->board_size / 2;
  } else if (game->move_history_count == 1) {
    // Second move - play adjacent to first
    find_first_ai_move(game, &best_x, &best_y);
  } else {
    // Use minimax
    find_best_ai_move(game, &best_x, &best_y);
  }

  double elapsed_time = get_current_time() - start_time;

  // Make the move
  if (best_x < 0 || best_y < 0) {
    log_error("AI failed to find valid move");
    cleanup_game(game);
    handle_internal_error(request, "AI failed to find a valid move");
    return;
  }

  // Get move scores for logging
  int own_score = evaluate_position(game->board, game->board_size, ai_player);
  int opp_score = evaluate_position(game->board, game->board_size, -ai_player);

  if (!make_move(game, best_x, best_y, ai_player, elapsed_time, 0, own_score,
                 opp_score)) {
    log_error("Failed to make move at [%d, %d]", best_x, best_y);
    cleanup_game(game);
    handle_internal_error(request, "Failed to apply AI move");
    return;
  }

  log_debug("AI move: [%d, %d] in %.3fs", best_x, best_y, elapsed_time);

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
    log_info("Game over: %s wins", winner);
  }

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
  log_debug("Not found");
  char *error_json = json_api_error_response("Not found");
  if (error_json) {
    send_json_response(request, 404, error_json);
    free(error_json);
  } else {
    send_json_response(request, 404, "{\"error\":\"Not found\"}");
  }
}

void handle_method_not_allowed(struct http_request_s *request) {
  log_debug("Method not allowed");
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
  log_warn("Bad request: %s", error_message);
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
  log_error("Internal error: %s", error_message);
  char *error_json = json_api_error_response(error_message);
  if (error_json) {
    send_json_response(request, 500, error_json);
    free(error_json);
  } else {
    send_json_response(request, 500, "{\"error\":\"Internal server error\"}");
  }
}
