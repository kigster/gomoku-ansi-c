//
//  test_client.c
//  test-gomoku-http - Test client for gomoku-http-daemon
//
//  Plays a complete game against the HTTP server
//

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "test_client_utils.h"

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 9900
#define BUFFER_SIZE 65536

// ANSI color codes
#define COLOR_YELLOW "\033[33m"
#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR_BOLD_YELLOW "\033[1;33m"
#define COLOR_BG_RED "\033[41m"
#define COLOR_RESET "\033[0m"

//===============================================================================
// HTTP ERROR TRACKING
//===============================================================================

#define MAX_ERROR_CODES 32

typedef struct {
  int status_code;
  int count;
} error_entry_t;

typedef struct {
  error_entry_t entries[MAX_ERROR_CODES];
  int num_entries;
} error_tracker_t;

static void error_tracker_record(error_tracker_t *tracker, int status_code) {
  if (!tracker || status_code < 100)
    return;
  for (int i = 0; i < tracker->num_entries; i++) {
    if (tracker->entries[i].status_code == status_code) {
      tracker->entries[i].count++;
      return;
    }
  }
  if (tracker->num_entries < MAX_ERROR_CODES) {
    tracker->entries[tracker->num_entries].status_code = status_code;
    tracker->entries[tracker->num_entries].count = 1;
    tracker->num_entries++;
  }
}

static int error_tracker_total(const error_tracker_t *tracker) {
  int total = 0;
  for (int i = 0; i < tracker->num_entries; i++) {
    total += tracker->entries[i].count;
  }
  return total;
}

// Forward declaration for use in http_post_with_retry
static void print_board_with_padding(const char *json, int padding, int red_bg);

//===============================================================================
// HTTP CLIENT
//===============================================================================

/**
 * Send HTTP POST request and receive response.
 * Returns response body (caller must free) or NULL on error.
 * If http_status is non-NULL, the HTTP status code is stored there.
 */
static char *http_post(const char *host, int port, const char *path,
                       const char *body, int *http_status) {
  // Create socket
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    fprintf(stderr, "Error: Failed to create socket: %s\n", strerror(errno));
    return NULL;
  }

  // Resolve host
  struct hostent *server = gethostbyname(host);
  if (!server) {
    fprintf(stderr, "Error: Failed to resolve host '%s'\n", host);
    close(sock);
    return NULL;
  }

  // Connect
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((uint16_t)port);
  memcpy(&addr.sin_addr.s_addr, server->h_addr, (size_t)server->h_length);

  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    fprintf(stderr, "Error: Failed to connect to %s:%d: %s\n", host, port,
            strerror(errno));
    close(sock);
    return NULL;
  }

  // Build HTTP request
  size_t body_len = strlen(body);
  char *request = malloc(body_len + 512);
  if (!request) {
    close(sock);
    return NULL;
  }

  int req_len = snprintf(request, body_len + 512,
                         "POST %s HTTP/1.1\r\n"
                         "Host: %s:%d\r\n"
                         "Content-Type: application/json\r\n"
                         "Content-Length: %zu\r\n"
                         "Connection: close\r\n"
                         "\r\n"
                         "%s",
                         path, host, port, body_len, body);

  // Send request
  if (send(sock, request, (size_t)req_len, 0) < 0) {
    fprintf(stderr, "Error: Failed to send request: %s\n", strerror(errno));
    free(request);
    close(sock);
    return NULL;
  }
  free(request);

  // Receive response
  char *response = malloc(BUFFER_SIZE);
  if (!response) {
    close(sock);
    return NULL;
  }

  size_t total = 0;
  ssize_t n;
  while ((n = recv(sock, response + total, BUFFER_SIZE - total - 1, 0)) > 0) {
    total += (size_t)n;
    if (total >= BUFFER_SIZE - 1)
      break;
  }
  response[total] = '\0';
  close(sock);

  // Find body (after \r\n\r\n)
  char *body_start = strstr(response, "\r\n\r\n");
  if (!body_start) {
    fprintf(stderr, "Error: Invalid HTTP response\n");
    free(response);
    return NULL;
  }
  body_start += 4;

  // Extract HTTP status code
  int status_code = 0;
  if (strncmp(response, "HTTP/1.1 ", 9) == 0 ||
      strncmp(response, "HTTP/1.0 ", 9) == 0) {
    status_code = atoi(response + 9);
  }

  if (http_status) {
    *http_status = status_code;
  }

  // Check for HTTP error status (but let caller handle retryable errors)
  if (status_code < 200 || status_code >= 300) {
    // Extract status line for error message
    char status_line[128];
    const char *end = strchr(response, '\r');
    size_t len = end ? (size_t)(end - response) : strlen(response);
    if (len >= sizeof(status_line))
      len = sizeof(status_line) - 1;
    memcpy(status_line, response, len);
    status_line[len] = '\0';

    // For 503, don't print error - caller will handle retry
    if (status_code != 503) {
      fprintf(stderr, "Error: Server returned: %s\n", status_line);
    }
    free(response);
    return NULL;
  }

  char *result = strdup(body_start);
  free(response);
  return result;
}

//===============================================================================
// HTTP POST WITH RETRY
//===============================================================================

/**
 * Send HTTP POST with exponential backoff retry on 503 errors.
 * Retries with delays: 0.1s, 0.2s, 0.4s, 0.8s, 1.6s, 3.2s, ...
 * Records all non-2xx status codes in the error tracker.
 * While retrying 503s, re-renders the board with a red background.
 * Returns response body (caller must free) or NULL on non-retryable error.
 */
static char *http_post_with_retry(const char *host, int port, const char *path,
                                  const char *body, int max_retries,
                                  error_tracker_t *tracker,
                                  const char *last_game_state, int padding) {
  double delay_sec = 0.1;
  int attempt = 0;

  while (1) {
    int http_status = 0;
    char *response = http_post(host, port, path, body, &http_status);

    if (response) {
      return response; // Success
    }

    // Record the error status code
    if (http_status >= 100) {
      error_tracker_record(tracker, http_status);
    }

    // Check if it's a 503 error (retryable)
    if (http_status != 503) {
      return NULL; // Non-retryable error
    }

    attempt++;
    if (max_retries > 0 && attempt >= max_retries) {
      return NULL;
    }

    // Re-render the board with red background to indicate 503 state
    if (last_game_state) {
      print_board_with_padding(last_game_state, padding, 1);
    }

    // Sleep with the current delay
    usleep((useconds_t)(delay_sec * 1000000));

    // Exponential backoff: double the delay for next attempt
    delay_sec *= 2.0;

    // Cap the delay at a reasonable maximum (e.g., 60 seconds)
    if (delay_sec > 60.0) {
      delay_sec = 60.0;
    }
  }
}

//===============================================================================
// GAME STATE PARSING
//===============================================================================

/**
 * Extract winner from JSON response.
 * Returns "none", "X", "O", or "draw".
 */
static const char *get_winner(const char *json) {
  const char *winner = strstr(json, "\"winner\"");
  if (!winner)
    return "none";

  winner = strchr(winner, ':');
  if (!winner)
    return "none";

  if (strstr(winner, "\"X\""))
    return "X";
  if (strstr(winner, "\"O\""))
    return "O";
  if (strstr(winner, "\"draw\""))
    return "draw";
  return "none";
}

/**
 * Print the board state from JSON response with colored pieces.
 * X is displayed in yellow, O is displayed in green.
 * If red_bg is non-zero, the entire board is rendered with a red background.
 */
static void print_board_with_padding(const char *json, int padding,
                                     int red_bg) {
  const char *board_state = strstr(json, "\"board_state\"");
  if (!board_state)
    return;

  // Find the opening bracket of the array
  const char *arr_start = strchr(board_state, '[');
  if (!arr_start)
    return;

  // Find the closing bracket
  const char *arr_end = strchr(arr_start, ']');
  if (!arr_end)
    return;

  if (padding < 0) {
    padding = 0;
  }

  // Clear screen and move cursor to top-left before printing the board
  printf("\033[2J\033[H");

  for (int i = 0; i < padding; i++) {
    printf("\n");
  }

  // Parse each line in the array
  const char *p = arr_start + 1;
  while (p < arr_end) {
    // Find the opening quote of the string
    const char *quote_start = strchr(p, '"');
    if (!quote_start || quote_start >= arr_end)
      break;

    // Find the closing quote
    const char *quote_end = strchr(quote_start + 1, '"');
    if (!quote_end || quote_end >= arr_end)
      break;

    // Print padding (with red background if active)
    if (red_bg)
      printf("%s", COLOR_BG_RED);
    printf("%*s", padding, "");

    // Print line content with colors for X and O
    for (const char *c = quote_start + 1; c < quote_end; c++) {
      if (*c == 'X') {
        printf("%s%sX%s", red_bg ? COLOR_BG_RED : "", COLOR_YELLOW,
               COLOR_RESET);
      } else if (*c == 'O') {
        printf("%s%sO%s", red_bg ? COLOR_BG_RED : "", COLOR_GREEN, COLOR_RESET);
      } else {
        putchar(*c);
      }
    }
    printf("%s\n", red_bg ? COLOR_RESET : "");

    p = quote_end + 1;
  }

  if (red_bg)
    printf("%s", COLOR_RESET);
}

//===============================================================================
// MAIN
//===============================================================================

/**
 * Save game state to JSON file, injecting server_errors if any were recorded.
 * Inserts "server_errors": { "503": N, ... } before the closing brace.
 */
static int save_game_json(const char *filename, const char *json,
                          const error_tracker_t *tracker) {
  FILE *fp = fopen(filename, "w");
  if (!fp) {
    fprintf(stderr, "Error: Failed to open '%s' for writing: %s\n", filename,
            strerror(errno));
    return 0;
  }

  if (tracker->num_entries == 0) {
    fprintf(fp, "%s", json);
    fclose(fp);
    return 1;
  }

  // Find the last '}' to inject server_errors before it
  const char *last_brace = strrchr(json, '}');
  if (!last_brace) {
    fprintf(fp, "%s", json);
    fclose(fp);
    return 1;
  }

  // Write everything up to the last '}'
  fwrite(json, 1, (size_t)(last_brace - json), fp);

  // Inject server_errors object
  fprintf(fp, ",\n  \"server_errors\": {");
  for (int i = 0; i < tracker->num_entries; i++) {
    fprintf(fp, "%s\n    \"%d\": %d", (i > 0) ? "," : "",
            tracker->entries[i].status_code, tracker->entries[i].count);
  }
  fprintf(fp, "\n  }\n}\n");

  fclose(fp);
  return 1;
}

static void print_usage(const char *program) {
  printf("test-gomoku-http - Test client for gomoku-http-daemon\n\n");
  printf("USAGE:\n");
  printf("  %s [options]\n\n", program);
  printf("OPTIONS:\n");
  printf("  -h, --host <host>     Server host (default: %s)\n", DEFAULT_HOST);
  printf("  -p, --port <port>     Server port (default: %d)\n", DEFAULT_PORT);
  printf("  -d, --depth <n>       AI search depth 1-6 (default: 2)\n");
  printf("  -r, --radius <n>      Search radius 1-4 (default: 2)\n");
  printf("  -b, --board <n>       Board size 15 or 19 (default: 15)\n");
  printf("  -j, --json <file>     Save game to JSON file when finished\n");
  printf("  -v, --verbose         Show game state after each move\n");
  printf("  --help                Show this help message\n\n");
  printf("EXAMPLE:\n");
  printf("  %s -h localhost -p 3000 -d 3 -r 2 -b 15 -j game.json\n", program);
}

int main(int argc, char *argv[]) {
  const char *host = DEFAULT_HOST;
  int port = DEFAULT_PORT;
  int depth = 2;
  int radius = 2;
  int board_size = 15;
  const char *json_file = NULL;
  int verbose = 0;

  static struct option long_options[] = {{"host", required_argument, 0, 'h'},
                                         {"port", required_argument, 0, 'p'},
                                         {"depth", required_argument, 0, 'd'},
                                         {"radius", required_argument, 0, 'r'},
                                         {"board", required_argument, 0, 'b'},
                                         {"json", required_argument, 0, 'j'},
                                         {"verbose", no_argument, 0, 'v'},
                                         {"help", no_argument, 0, '?'},
                                         {0, 0, 0, 0}};

  int c;
  while ((c = getopt_long(argc, argv, "h:p:d:r:b:j:v", long_options, NULL)) !=
         -1) {
    switch (c) {
    case 'h':
      host = optarg;
      break;
    case 'p':
      port = atoi(optarg);
      if (port <= 0 || port > 65535) {
        fprintf(stderr, "Error: Invalid port number\n");
        return 1;
      }
      break;
    case 'd':
      depth = atoi(optarg);
      if (depth < 1 || depth > 6) {
        fprintf(stderr, "Error: Depth must be 1-6\n");
        return 1;
      }
      break;
    case 'r':
      radius = atoi(optarg);
      if (radius < 1 || radius > 4) {
        fprintf(stderr, "Error: Radius must be 1-4\n");
        return 1;
      }
      break;
    case 'b':
      board_size = atoi(optarg);
      if (board_size != 15 && board_size != 19) {
        fprintf(stderr, "Error: Board size must be 15 or 19\n");
        return 1;
      }
      break;
    case 'j':
      json_file = optarg;
      break;
    case 'v':
      verbose = 1;
      break;
    case '?':
    default:
      print_usage(argv[0]);
      return (c == '?') ? 0 : 1;
    }
  }

  // Disable output buffering for real-time display
  setvbuf(stdout, NULL, _IONBF, 0);

  printf("Connecting to gomoku-http-daemon at %s:%d\n", host, port);
  printf("Server plays both sides (depth=%d, radius=%d, board=%d)\n\n", depth,
         radius, board_size);

  // Start with initial game state - human makes first move
  char *game_state =
      test_client_create_initial_game_state(board_size, depth, radius);
  if (!game_state) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    return 1;
  }

  int move_num = 0;
  const char *winner = "none";
  error_tracker_t errors = {0};

  while (strcmp(winner, "none") == 0) {
    // Send to server (with retry on 503 errors; board turns red during retries)
    char *response = http_post_with_retry(
        host, port, "/gomoku/play", game_state, 0, &errors, game_state, 3);
    if (!response) {
      fprintf(stderr, "Error: Failed to communicate with server\n");
      free(game_state);
      return 1;
    }

    free(game_state);
    game_state = response;

    // Always print the board with padding (normal background)
    print_board_with_padding(game_state, 3, 0);

    move_num++;
    const char *label = NULL;
    int x = 0, y = 0;
    // Print move info below the board (with left padding to match)
    if (test_client_get_last_move(game_state, &label, &x, &y)) {
      printf("\n%*sMove %d: %s plays [%d, %d]\n", 3, "", move_num, label, x, y);
    } else {
      printf("\n%*sMove %d: (unable to parse last move)\n", 3, "", move_num);
    }

    if (verbose) {
      // Additional debug info in verbose mode could go here
    }

    // Check for winner after move
    winner = get_winner(game_state);
  }

  // Print the final board (already printed in loop, but print again for
  // clarity)
  print_board_with_padding(game_state, 3, 0);

  printf("\n");
  if (strcmp(winner, "X") == 0) {
    printf("%*sGame over: X wins!\n", 3, "");
  } else if (strcmp(winner, "O") == 0) {
    printf("%*sGame over: O wins!\n", 3, "");
  } else if (strcmp(winner, "draw") == 0) {
    printf("%*sGame over: Draw!\n", 3, "");
  }

  printf("%*sTotal moves: %d\n", 3, "", move_num);

  // Print server error summary if any occurred
  if (error_tracker_total(&errors) > 0) {
    printf("%*sServer errors: %d total", 3, "", error_tracker_total(&errors));
    for (int i = 0; i < errors.num_entries; i++) {
      printf("%s %d=%d", (i > 0) ? "," : " (", errors.entries[i].status_code,
             errors.entries[i].count);
    }
    printf(")\n");
  }

  // Save game to JSON file if specified
  if (json_file) {
    if (save_game_json(json_file, game_state, &errors)) {
      printf("%*sGame saved to: %s\n", 3, "", json_file);
    }
  }

  free(game_state);
  return 0;
}
