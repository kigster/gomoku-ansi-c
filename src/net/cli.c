//
//  cli.c
//  gomoku-httpd - CLI argument parsing for network daemon
//

#include "cli.h"
#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//===============================================================================
// INTERNAL HELPERS
//===============================================================================

/**
 * Parse bind address in format "host:port" or just "port".
 * Returns 1 on success, 0 on failure.
 */
static int parse_bind_address(const char *bind_str, char *host, size_t host_len,
                              int *port) {
  if (!bind_str || !host || !port) {
    return 0;
  }

  // Check for colon separator
  const char *colon = strrchr(bind_str, ':');

  if (colon) {
    // Format: host:port
    size_t host_part_len = (size_t)(colon - bind_str);
    if (host_part_len >= host_len) {
      return 0;
    }
    strncpy(host, bind_str, host_part_len);
    host[host_part_len] = '\0';

    *port = atoi(colon + 1);
  } else {
    // Format: just port number
    strncpy(host, "0.0.0.0", host_len - 1);
    host[host_len - 1] = '\0';
    *port = atoi(bind_str);
  }

  // Validate port
  if (*port <= 0 || *port > 65535) {
    return 0;
  }

  return 1;
}

//===============================================================================
// PUBLIC FUNCTIONS
//===============================================================================

daemon_log_level_t daemon_parse_log_level(const char *level_str) {
  if (!level_str) {
    return -1;
  }

  // Create uppercase copy for comparison
  char upper[16];
  size_t len = strlen(level_str);
  if (len >= sizeof(upper)) {
    return -1;
  }

  for (size_t i = 0; i <= len; i++) {
    upper[i] = (char)toupper((unsigned char)level_str[i]);
  }

  if (strcmp(upper, "TRACE") == 0)
    return DAEMON_LOG_TRACE;
  if (strcmp(upper, "DEBUG") == 0)
    return DAEMON_LOG_DEBUG;
  if (strcmp(upper, "INFO") == 0)
    return DAEMON_LOG_INFO;
  if (strcmp(upper, "WARN") == 0)
    return DAEMON_LOG_WARN;
  if (strcmp(upper, "WARNING") == 0)
    return DAEMON_LOG_WARN;
  if (strcmp(upper, "ERROR") == 0)
    return DAEMON_LOG_ERROR;
  if (strcmp(upper, "FATAL") == 0)
    return DAEMON_LOG_FATAL;

  return -1;
}

daemon_config_t daemon_parse_arguments(int argc, char *argv[]) {
  daemon_config_t config = {
      .bind_host = "",
      .bind_port = 0,
      .agent_port = 0,
      .daemonize = 0,
      .log_file = "",
      .log_level = DAEMON_LOG_INFO,
      .show_help = 0,
      .invalid_args = 0,
  };

  static struct option long_options[] = {
      {"bind", required_argument, 0, 'b'},
      {"agent-port", required_argument, 0, 'a'},
      {"daemonize", no_argument, 0, 'd'},
      {"log-file", required_argument, 0, 'l'},
      {"log-level", required_argument, 0, 'L'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  int c;
  int option_index = 0;

  // Reset getopt
  optind = 1;

  while ((c = getopt_long(argc, argv, "b:a:dl:L:h", long_options,
                          &option_index)) != -1) {
    switch (c) {
    case 'b':
      if (!parse_bind_address(optarg, config.bind_host,
                              sizeof(config.bind_host), &config.bind_port)) {
        fprintf(stderr, "Error: Invalid bind address '%s'\n", optarg);
        fprintf(stderr, "Expected format: host:port or just port\n");
        config.invalid_args = 1;
      }
      break;

    case 'a': {
      int port = atoi(optarg);
      if (port <= 0 || port > 65535) {
        fprintf(stderr, "Error: Invalid agent port '%s'\n", optarg);
        fprintf(stderr, "Expected port number between 1 and 65535\n");
        config.invalid_args = 1;
      } else {
        config.agent_port = port;
      }
      break;
    }

    case 'd':
      config.daemonize = 1;
      break;

    case 'l':
      strncpy(config.log_file, optarg, sizeof(config.log_file) - 1);
      config.log_file[sizeof(config.log_file) - 1] = '\0';
      break;

    case 'L': {
      daemon_log_level_t level = daemon_parse_log_level(optarg);
      if ((int)level < 0) {
        fprintf(stderr, "Error: Invalid log level '%s'\n", optarg);
        fprintf(stderr,
                "Valid levels: TRACE, DEBUG, INFO, WARN, ERROR, FATAL\n");
        config.invalid_args = 1;
      } else {
        config.log_level = level;
      }
      break;
    }

    case 'h':
      config.show_help = 1;
      break;

    case '?':
    default:
      config.invalid_args = 1;
      break;
    }
  }

  return config;
}

int daemon_validate_config(const daemon_config_t *config) {
  if (!config) {
    return 0;
  }

  // Show help doesn't need validation
  if (config->show_help) {
    return 1;
  }

  // Invalid args already detected
  if (config->invalid_args) {
    return 0;
  }

  // Bind address is required
  if (config->bind_port == 0) {
    fprintf(stderr, "Error: Bind address is required (-b/--bind)\n");
    return 0;
  }

  return 1;
}

void daemon_print_help(const char *program_name) {
  printf("gomoku-httpd v%s - Gomoku AI HTTP Server\n\n", DAEMON_VERSION);

  printf("USAGE:\n");
  printf("  %s -b <host:port> [options]\n\n", program_name);

  printf("REQUIRED:\n");
  printf("  -b, --bind <host:port>   Address to bind (e.g., 0.0.0.0:3000)\n");
  printf("                           Can also be just port (e.g., 3000)\n\n");

  printf("OPTIONS:\n");
  printf("  -a, --agent-port <port>  HAProxy agent-check port (default: "
         "disabled)\n");
  printf("                           Enables health reporting for load "
         "balancers\n");
  printf("  -d, --daemonize          Run as a background daemon\n");
  printf("  -l, --log-file <file>    Log to file instead of stdout\n");
  printf("  -L, --log-level <level>  Set log level (default: INFO)\n");
  printf("                           Levels: TRACE, DEBUG, INFO, WARN, ERROR, "
         "FATAL\n");
  printf("  -h, --help               Show this help message\n\n");

  printf("ENDPOINTS:\n");
  printf("  GET  /health             Liveness check (always 200 if alive)\n");
  printf("  GET  /ready              Readiness check (200=idle, 503=busy)\n");
  printf("  POST /gomoku/play        Make AI move (accepts/returns JSON)\n\n");

  printf("HAPROXY AGENT-CHECK:\n");
  printf(
      "  When --agent-port is specified, a lightweight TCP server runs on\n");
  printf("  that port responding with 'ready' (idle) or 'drain' (busy).\n");
  printf(
      "  This allows HAProxy to route requests only to available servers.\n\n");

  printf("EXAMPLES:\n");
  printf("  %s -b 3000                          # Listen on all interfaces, "
         "port 3000\n",
         program_name);
  printf("  %s -b 127.0.0.1:8080                # Listen on localhost only\n",
         program_name);
  printf("  %s -b 0.0.0.0:3000 -d               # Run as daemon\n",
         program_name);
  printf("  %s -b 3000 -l /var/log/gomoku.log   # Log to file\n", program_name);
  printf("  %s -b 3000 -L DEBUG                 # Enable debug logging\n",
         program_name);
  printf("  %s -b 8787 -a 8788                  # With HAProxy agent-check\n\n",
         program_name);

  printf("CONSTRAINTS:\n");
  printf("  Max AI depth: %d\n", 6);
  printf("  Max search radius: %d\n", 4);
  printf("  Single-threaded (one request at a time)\n");
}
