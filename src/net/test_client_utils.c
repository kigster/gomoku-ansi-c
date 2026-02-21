//
//  test_client_utils.c
//  Shared helpers for gomoku-http-client and tests
//

#include "test_client_utils.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *last_strstr(const char *haystack, const char *needle) {
  const char *last = NULL;
  const char *p = haystack;
  while ((p = strstr(p, needle)) != NULL) {
    last = p;
    p++;
  }
  return last;
}

static int column_index_from_char(char c) {
  static const char *columns = "ABCDEFGHJKLMNOPQRST";
  char upper = (char)toupper((unsigned char)c);
  for (int i = 0; columns[i] != '\0'; i++) {
    if (columns[i] == upper) {
      return i;
    }
  }
  return -1;
}

static int parse_coord_notation(const char *s, int *x, int *y) {
  if (!s || !x || !y || strlen(s) < 2) {
    return 0;
  }
  int col = column_index_from_char(s[0]);
  if (col < 0) {
    return 0;
  }
  for (size_t i = 1; s[i] != '\0'; i++) {
    if (!isdigit((unsigned char)s[i])) {
      return 0;
    }
  }
  *x = atoi(s + 1);
  *y = col;
  return 1;
}

int test_client_get_last_move(const char *json, const char **out_label,
                              int *out_x, int *out_y) {
  const char *labels[] = {"\"X (AI)\"", "\"O (AI)\"", "\"X (human)\"",
                          "\"O (human)\""};
  const char *last = NULL;
  const char *last_label = NULL;

  for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); i++) {
    const char *found = last_strstr(json, labels[i]);
    if (found && (!last || found > last)) {
      last = found;
      last_label = labels[i];
    }
  }

  if (!last || !last_label) {
    return 0;
  }

  const char *colon = strchr(last, ':');
  if (!colon) {
    return 0;
  }

  while (*colon && (*colon == ':' || isspace((unsigned char)*colon))) {
    colon++;
  }

  int x = -1, y = -1;
  if (*colon == '"') {
    char coord[16];
    const char *start = colon + 1;
    const char *end = strchr(start, '"');
    if (!end) {
      return 0;
    }
    size_t len = (size_t)(end - start);
    if (len == 0 || len >= sizeof(coord)) {
      return 0;
    }
    memcpy(coord, start, len);
    coord[len] = '\0';
    if (!parse_coord_notation(coord, &x, &y)) {
      return 0;
    }
  } else {
    const char *pos = strchr(colon, '[');
    if (!pos || sscanf(pos, "[%d, %d]", &x, &y) != 2) {
      return 0;
    }
  }

  if (strcmp(last_label, "\"X (AI)\"") == 0) {
    *out_label = "X (AI)";
  } else if (strcmp(last_label, "\"O (AI)\"") == 0) {
    *out_label = "O (AI)";
  } else if (strcmp(last_label, "\"X (human)\"") == 0) {
    *out_label = "X (human)";
  } else if (strcmp(last_label, "\"O (human)\"") == 0) {
    *out_label = "O (human)";
  } else {
    *out_label = "unknown";
  }

  *out_x = x;
  *out_y = y;
  return 1;
}

char *test_client_create_initial_game_state(int board_size, int depth,
                                            int radius) {
  return test_client_create_initial_game_state_ex(board_size, depth, depth,
                                                  radius, 0);
}

char *test_client_create_initial_game_state_ex(int board_size, int depth_x,
                                               int depth_o, int radius,
                                               int timeout_sec) {
  char *json = malloc(640);
  if (!json)
    return NULL;

  if (timeout_sec > 0) {
    snprintf(
        json, 640,
        "{\n"
        "  \"X\": { \"player\": \"AI\", \"depth\": %d, \"time_ms\": 0.000 },\n"
        "  \"O\": { \"player\": \"AI\", \"depth\": %d, \"time_ms\": 0.000 },\n"
        "  \"board_size\": %d,\n"
        "  \"radius\": %d,\n"
        "  \"timeout\": %d,\n"
        "  \"winner\": \"none\",\n"
        "  \"board_state\": [],\n"
        "  \"moves\": []\n"
        "}\n",
        depth_x, depth_o, board_size, radius, timeout_sec);
  } else {
    snprintf(
        json, 640,
        "{\n"
        "  \"X\": { \"player\": \"AI\", \"depth\": %d, \"time_ms\": 0.000 },\n"
        "  \"O\": { \"player\": \"AI\", \"depth\": %d, \"time_ms\": 0.000 },\n"
        "  \"board_size\": %d,\n"
        "  \"radius\": %d,\n"
        "  \"timeout\": \"none\",\n"
        "  \"winner\": \"none\",\n"
        "  \"board_state\": [],\n"
        "  \"moves\": []\n"
        "}\n",
        depth_x, depth_o, board_size, radius);
  }

  return json;
}
