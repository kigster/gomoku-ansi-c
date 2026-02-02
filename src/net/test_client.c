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

//===============================================================================
// HTTP CLIENT
//===============================================================================

/**
 * Send HTTP POST request and receive response.
 * Returns response body (caller must free) or NULL on error.
 */
static char *http_post(const char *host, int port, const char *path,
                       const char *body) {
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

  // Check for HTTP error status
  if (strncmp(response, "HTTP/1.1 2", 10) != 0 &&
      strncmp(response, "HTTP/1.0 2", 10) != 0) {
    // Extract status line for error message
    char *end = strchr(response, '\r');
    if (end)
      *end = '\0';
    fprintf(stderr, "Error: Server returned: %s\n", response);
    free(response);
    return NULL;
  }

  char *result = strdup(body_start);
  free(response);
  return result;
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

// ANSI color codes
#define COLOR_RED "\033[31m"
#define COLOR_BLUE "\033[34m"
#define COLOR_RESET "\033[0m"

/**
 * Print the board state from JSON response with colored pieces.
 * X is displayed in red, O is displayed in blue.
 */
static void print_board_with_padding(const char *json, int padding) {
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

    // Print padding
    printf("%*s", padding, "");

    // Print line content with colors for X and O
    for (const char *c = quote_start + 1; c < quote_end; c++) {
      if (*c == 'X') {
        printf("%sX%s", COLOR_RED, COLOR_RESET);
      } else if (*c == 'O') {
        printf("%sO%s", COLOR_BLUE, COLOR_RESET);
      } else {
        putchar(*c);
      }
    }
    printf("\n");

    p = quote_end + 1;
  }
}

//===============================================================================
// MAIN
//===============================================================================

/**
 * Save game state to JSON file.
 */
static int save_game_json(const char *filename, const char *json) {
  FILE *fp = fopen(filename, "w");
  if (!fp) {
    fprintf(stderr, "Error: Failed to open '%s' for writing: %s\n", filename,
            strerror(errno));
    return 0;
  }
  fprintf(fp, "%s", json);
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

  while (strcmp(winner, "none") == 0) {
    // Send to server
    char *response = http_post(host, port, "/gomoku/play", game_state);
    if (!response) {
      fprintf(stderr, "Error: Failed to communicate with server\n");
      free(game_state);
      return 1;
    }

    free(game_state);
    game_state = response;

    // Always print the board with padding
    print_board_with_padding(game_state, 3);

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
  print_board_with_padding(game_state, 3);

  printf("\n");
  if (strcmp(winner, "X") == 0) {
    printf("%*sGame over: X wins!\n", 3, "");
  } else if (strcmp(winner, "O") == 0) {
    printf("%*sGame over: O wins!\n", 3, "");
  } else if (strcmp(winner, "draw") == 0) {
    printf("%*sGame over: Draw!\n", 3, "");
  }

  printf("%*sTotal moves: %d\n", 3, "", move_num);

  // Save game to JSON file if specified
  if (json_file) {
    if (save_game_json(json_file, game_state)) {
      printf("%*sGame saved to: %s\n", 3, "", json_file);
    }
  }

  free(game_state);
  return 0;
}
