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

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 3000
#define BUFFER_SIZE 65536
#define BOARD_SIZE 19

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

/**
 * Print the board state from JSON response.
 */
static void print_board(const char *json) {
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

  printf("\nFinal board:\n");

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

    // Print the line content
    printf("  %.*s\n", (int)(quote_end - quote_start - 1), quote_start + 1);

    p = quote_end + 1;
  }
}

/**
 * Check if a position is occupied on the board.
 * Simple parser - looks for the position in moves array.
 * Handles both compact [9, 9] and expanded JSON formats.
 */
static int is_position_taken(const char *json, int x, int y) {
  char pattern1[32], pattern2[64];

  // Try compact format: [9, 9]
  snprintf(pattern1, sizeof(pattern1), "[%d, %d]", x, y);
  if (strstr(json, pattern1) != NULL) {
    return 1;
  }

  // Try expanded format from json-c:
  // [\n        9,\n        9\n      ]
  snprintf(pattern2, sizeof(pattern2), "[\n        %d,\n        %d\n      ]", x,
           y);
  if (strstr(json, pattern2) != NULL) {
    return 1;
  }

  return 0;
}

/**
 * Find a valid move for the human player.
 * Uses a simple strategy: play near the center or near existing moves.
 */
static int find_human_move(const char *json, int board_sz, int *out_x,
                           int *out_y) {
  int center = board_sz / 2;

  // Try center first
  if (!is_position_taken(json, center, center)) {
    *out_x = center;
    *out_y = center;
    return 1;
  }

  // Spiral out from center
  for (int r = 1; r < board_sz; r++) {
    for (int dx = -r; dx <= r; dx++) {
      for (int dy = -r; dy <= r; dy++) {
        if (abs(dx) != r && abs(dy) != r)
          continue;

        int x = center + dx;
        int y = center + dy;

        if (x >= 0 && x < board_sz && y >= 0 && y < board_sz) {
          if (!is_position_taken(json, x, y)) {
            *out_x = x;
            *out_y = y;
            return 1;
          }
        }
      }
    }
  }

  return 0;
}

/**
 * Add a human move to the JSON game state.
 * Returns new JSON string (caller must free) or NULL on error.
 */
static char *add_human_move(const char *json, int x, int y) {
  // Find the moves array end
  const char *moves_end = strstr(json, "\"moves\"");
  if (!moves_end)
    return NULL;

  // Find the closing bracket of moves array
  const char *bracket = strrchr(moves_end, ']');
  if (!bracket)
    return NULL;

  // Calculate position
  size_t prefix_len = (size_t)(bracket - json);
  size_t suffix_len = strlen(bracket);

  // Check if moves array is empty
  const char *moves_start = strchr(moves_end, '[');
  int is_empty = 1;
  if (moves_start) {
    for (const char *p = moves_start + 1; p < bracket; p++) {
      if (*p != ' ' && *p != '\n' && *p != '\r' && *p != '\t') {
        is_empty = 0;
        break;
      }
    }
  }

  // Build new move JSON
  char move_json[128];
  if (is_empty) {
    snprintf(move_json, sizeof(move_json),
             "\n    { \"X (human)\": [%d, %d], \"time_ms\": 0.000 }\n  ", x, y);
  } else {
    snprintf(move_json, sizeof(move_json),
             ",\n    { \"X (human)\": [%d, %d], \"time_ms\": 0.000 }\n  ", x,
             y);
  }

  // Build new JSON
  size_t new_len = prefix_len + strlen(move_json) + suffix_len + 1;
  char *new_json = malloc(new_len);
  if (!new_json)
    return NULL;

  memcpy(new_json, json, prefix_len);
  strcpy(new_json + prefix_len, move_json);
  strcat(new_json, bracket);

  return new_json;
}

//===============================================================================
// INITIAL GAME STATE
//===============================================================================

static char *create_initial_game_state(int board_size, int depth, int radius) {
  char *json = malloc(512);
  if (!json)
    return NULL;

  snprintf(
      json, 512,
      "{\n"
      "  \"X\": { \"player\": \"human\", \"time_ms\": 0.000 },\n"
      "  \"O\": { \"player\": \"AI\", \"depth\": %d, \"time_ms\": 0.000 },\n"
      "  \"board\": %d,\n"
      "  \"radius\": %d,\n"
      "  \"timeout\": \"none\",\n"
      "  \"winner\": \"none\",\n"
      "  \"board_state\": [],\n"
      "  \"moves\": []\n"
      "}\n",
      depth, board_size, radius);

  return json;
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

  printf("Connecting to gomoku-http-daemon at %s:%d\n", host, port);
  printf(
      "Playing as X (human) against O (AI depth=%d, radius=%d, board=%d)\n\n",
      depth, radius, board_size);

  // Start with initial game state - human makes first move
  char *game_state = create_initial_game_state(board_size, depth, radius);
  if (!game_state) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    return 1;
  }

  int move_num = 0;
  const char *winner = "none";

  while (strcmp(winner, "none") == 0) {
    move_num++;

    // Find and add human move
    int hx, hy;
    if (!find_human_move(game_state, board_size, &hx, &hy)) {
      printf("No valid moves available - game is a draw\n");
      break;
    }

    char *with_move = add_human_move(game_state, hx, hy);
    free(game_state);
    if (!with_move) {
      fprintf(stderr, "Error: Failed to add move\n");
      return 1;
    }
    game_state = with_move;

    printf("Move %d: X plays [%d, %d]\n", move_num, hx, hy);

    // Send to server
    char *response = http_post(host, port, "/gomoku/play", game_state);
    if (!response) {
      fprintf(stderr, "Error: Failed to communicate with server\n");
      free(game_state);
      return 1;
    }

    free(game_state);
    game_state = response;

    if (verbose) {
      printf("Server response:\n%s\n", game_state);
    }

    // Check for winner after human move
    winner = get_winner(game_state);
    if (strcmp(winner, "none") != 0) {
      break;
    }

    // Server made AI move, extract and display it
    // Look for the last O move
    const char *last_o = game_state;
    const char *found;
    while ((found = strstr(last_o + 1, "\"O (AI)\"")) != NULL) {
      last_o = found;
    }
    if (last_o != game_state) {
      // Parse position
      const char *pos = strchr(last_o, '[');
      if (pos) {
        int ox, oy;
        if (sscanf(pos, "[%d, %d]", &ox, &oy) == 2) {
          move_num++;
          printf("Move %d: O plays [%d, %d]\n", move_num, ox, oy);
        }
      }
    }

    // Check for winner after AI move
    winner = get_winner(game_state);
  }

  // Print the final board
  print_board(game_state);

  printf("\n");
  if (strcmp(winner, "X") == 0) {
    printf("Game over: X (human) wins!\n");
  } else if (strcmp(winner, "O") == 0) {
    printf("Game over: O (AI) wins!\n");
  } else if (strcmp(winner, "draw") == 0) {
    printf("Game over: Draw!\n");
  }

  printf("Total moves: %d\n", move_num);

  // Save game to JSON file if specified
  if (json_file) {
    if (save_game_json(json_file, game_state)) {
      printf("Game saved to: %s\n", json_file);
    }
  }

  free(game_state);
  return 0;
}
