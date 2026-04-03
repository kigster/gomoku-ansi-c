//
//  test_client_utils.c
//  Shared helpers for gomoku-http-client and tests
//

#include "test_client_utils.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define HTTP_HEADER_MAX 8192
#define HTTP_FALLBACK_BODY_SIZE (512 * 1024)
#define HTTP_DEFAULT_MAX_BODY_SIZE (2 * 1024 * 1024)

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
  int row = atoi(s + 1);
  if (row < 1) {
    return 0;
  }
  *x = row - 1;
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

/**
 * Parse Content-Length from HTTP header block. Returns -1 if not found or
 * invalid; otherwise returns the value (may be 0).
 */
static long parse_content_length(const char *headers, size_t headers_len) {
  const char *end = headers + headers_len;
  const char *q = strstr(headers, "Content-Length:");
  if (!q || q + 14 > end)
    return -1;
  q += 14;
  while (q < end && (*q == ' ' || *q == '\t'))
    q++;
  if (q >= end || !isdigit((unsigned char)*q))
    return -1;
  char *endptr = NULL;
  long val = strtol(q, &endptr, 10);
  if (!endptr || endptr <= q || val < 0)
    return -1;
  return val;
}

char *test_client_read_http_response(int sock, int *out_status,
                                     size_t *out_body_len, size_t max_body_size,
                                     void (*tick_cb)(void *), void *tick_ctx) {
  if (max_body_size == 0)
    max_body_size = HTTP_DEFAULT_MAX_BODY_SIZE;

  char *header_buf = malloc(HTTP_HEADER_MAX + 1);
  if (!header_buf)
    return NULL;

  size_t header_len = 0;
  int found_blank = 0;
  while (header_len < HTTP_HEADER_MAX) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    struct timeval tv = {1, 0};
    int ready = select(sock + 1, &readfds, NULL, NULL, &tv);
    if (ready > 0) {
      size_t to_read = HTTP_HEADER_MAX - header_len;
      if (to_read > 256)
        to_read = 256;
      ssize_t n = recv(sock, header_buf + header_len, to_read, 0);
      if (n <= 0)
        break;
      header_len += (size_t)n;
      header_buf[header_len] = '\0';
      if (header_len >= 4 && strstr(header_buf, "\r\n\r\n") != NULL) {
        found_blank = 1;
        break;
      }
    } else if (ready == 0) {
      if (tick_cb)
        tick_cb(tick_ctx);
    } else {
      if (errno == EINTR)
        continue;
      break;
    }
  }

  if (!found_blank) {
    free(header_buf);
    return NULL;
  }

  int status = 0;
  if (header_len >= 9 && (memcmp(header_buf, "HTTP/1.1 ", 9) == 0 ||
                          memcmp(header_buf, "HTTP/1.0 ", 9) == 0))
    status = atoi(header_buf + 9);

  if (out_status)
    *out_status = status;

  long content_length = parse_content_length(header_buf, header_len);

  /* Body may have started in the same read as headers; don't discard it */
  const char *body_start = strstr(header_buf, "\r\n\r\n");
  if (body_start)
    body_start += 4;
  size_t body_in_header = (body_start && body_start <= header_buf + header_len)
                              ? (size_t)((header_buf + header_len) - body_start)
                              : 0;

  char *body = NULL;
  size_t body_cap = 0;
  size_t body_len = 0;

  if (content_length >= 0 && (size_t)content_length <= max_body_size) {
    body_cap = (size_t)content_length + 1;
    body = malloc(body_cap);
    if (!body) {
      free(header_buf);
      return NULL;
    }
    /* Copy any body bytes we already received with the headers */
    if (body_in_header > 0 && body_in_header <= (size_t)content_length) {
      memcpy(body, body_start, body_in_header);
      body_len = body_in_header;
    }
    free(header_buf);
    header_buf = NULL;

    while (body_len < (size_t)content_length) {
      fd_set readfds;
      FD_ZERO(&readfds);
      FD_SET(sock, &readfds);
      struct timeval tv = {1, 0};
      int ready = select(sock + 1, &readfds, NULL, NULL, &tv);
      if (ready > 0) {
        ssize_t n =
            recv(sock, body + body_len, (size_t)content_length - body_len, 0);
        if (n <= 0) {
          free(body);
          return NULL;
        }
        body_len += (size_t)n;
      } else if (ready == 0) {
        if (tick_cb)
          tick_cb(tick_ctx);
      } else {
        if (errno == EINTR)
          continue;
        free(body);
        return NULL;
      }
    }
    body[body_len] = '\0';
  } else {
    body_cap = HTTP_FALLBACK_BODY_SIZE + 1;
    body = malloc(body_cap);
    if (!body) {
      free(header_buf);
      return NULL;
    }
    /* Copy any body bytes we already received with the headers */
    if (body_in_header > 0 && body_in_header <= HTTP_FALLBACK_BODY_SIZE) {
      memcpy(body, body_start, body_in_header);
      body_len = body_in_header;
    }
    free(header_buf);
    header_buf = NULL;

    while (body_len < HTTP_FALLBACK_BODY_SIZE) {
      fd_set readfds;
      FD_ZERO(&readfds);
      FD_SET(sock, &readfds);
      struct timeval tv = {1, 0};
      int ready = select(sock + 1, &readfds, NULL, NULL, &tv);
      if (ready > 0) {
        ssize_t n =
            recv(sock, body + body_len, HTTP_FALLBACK_BODY_SIZE - body_len, 0);
        if (n <= 0)
          break;
        body_len += (size_t)n;
      } else if (ready == 0) {
        if (tick_cb)
          tick_cb(tick_ctx);
      } else {
        if (errno == EINTR)
          continue;
        break;
      }
    }
    body[body_len] = '\0';
  }

  if (out_body_len)
    *out_body_len = body_len;

  return body;
}

int test_client_response_looks_complete(const char *body, size_t body_len) {
  if (!body || body_len == 0)
    return 0;
  const char *p = body + body_len - 1;
  while (p > body && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
    p--;
  return (p >= body && *p == '}');
}
