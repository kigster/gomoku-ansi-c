//
//  handlers.h
//  gomoku-http-daemon - HTTP endpoint handlers
//

#ifndef NET_HANDLERS_H
#define NET_HANDLERS_H

// Forward declaration - httpserver.h is included in handlers.c with HTTPSERVER_IMPL
struct http_request_s;

//===============================================================================
// INITIALIZATION
//===============================================================================

/**
 * Initialize handlers. Must be called once at startup.
 * Calls populate_threat_matrix() and any other required initialization.
 */
void handlers_init(void);

//===============================================================================
// REQUEST DISPATCHER
//===============================================================================

/**
 * Main request dispatcher. Routes requests to appropriate handlers.
 *
 * @param request The HTTP request
 */
void handle_request(struct http_request_s *request);

//===============================================================================
// ENDPOINT HANDLERS
//===============================================================================

/**
 * Handle GET /health endpoint.
 * Returns JSON: {"status": "ok", "version": "1.0.0", "uptime": "..."}
 *
 * @param request The HTTP request
 */
void handle_health(struct http_request_s *request);

/**
 * Handle POST /gomoku/play endpoint.
 * Receives game state JSON, makes AI move, returns updated JSON.
 *
 * @param request The HTTP request
 */
void handle_play(struct http_request_s *request);

/**
 * Handle 404 Not Found responses.
 *
 * @param request The HTTP request
 */
void handle_not_found(struct http_request_s *request);

/**
 * Handle 405 Method Not Allowed responses.
 *
 * @param request The HTTP request
 */
void handle_method_not_allowed(struct http_request_s *request);

/**
 * Handle 400 Bad Request responses.
 *
 * @param request The HTTP request
 * @param error_message Error message to include in response
 */
void handle_bad_request(struct http_request_s *request, const char *error_message);

/**
 * Handle 500 Internal Server Error responses.
 *
 * @param request The HTTP request
 * @param error_message Error message to include in response
 */
void handle_internal_error(struct http_request_s *request, const char *error_message);

#endif // NET_HANDLERS_H
