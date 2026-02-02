//
//  test_client_utils.c
//  Shared helpers for gomoku-http-client and tests
//

#include "test_client_utils.h"
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

  const char *pos = strchr(last, '[');
  if (!pos) {
    return 0;
  }

  int x, y;
  if (sscanf(pos, "[%d, %d]", &x, &y) != 2) {
    return 0;
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
  char *json = malloc(512);
  if (!json)
    return NULL;

  snprintf(
      json, 512,
      "{\n"
      "  \"X\": { \"player\": \"AI\", \"depth\": %d, \"time_ms\": 0.000 },\n"
      "  \"O\": { \"player\": \"AI\", \"depth\": %d, \"time_ms\": 0.000 },\n"
      "  \"board\": %d,\n"
      "  \"radius\": %d,\n"
      "  \"timeout\": \"none\",\n"
      "  \"winner\": \"none\",\n"
      "  \"board_state\": [],\n"
      "  \"moves\": []\n"
      "}\n",
      depth, depth, board_size, radius);

  return json;
}
