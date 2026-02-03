//
//  main.c
//  gomoku-httpd - HTTP server entry point
//

#include "cli.h"
#include "handlers.h"
#include "httpserver.h"
#include "logger.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

//===============================================================================
// GLOBAL STATE
//===============================================================================

static volatile sig_atomic_t running = 1;

// Agent thread state
static pthread_t agent_thread_id;
static int agent_socket_fd = -1;
static volatile sig_atomic_t agent_running = 0;

//===============================================================================
// PORT AVAILABILITY CHECK
//===============================================================================

/**
 * Check if a port is available for binding.
 * Returns 1 if available, 0 if in use, -1 on error.
 */
static int check_port_available(const char *host, int port) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    return -1;
  }

  // Set SO_REUSEADDR but NOT SO_REUSEPORT
  // This will fail if another process has SO_REUSEPORT on this port
  int opt = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((uint16_t)port);

  if (host && strlen(host) > 0 && strcmp(host, "0.0.0.0") != 0) {
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
      close(sock);
      return -1;
    }
  } else {
    addr.sin_addr.s_addr = INADDR_ANY;
  }

  int result = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
  int bind_errno = errno;
  close(sock);

  if (result == 0) {
    return 1; // Port is available
  }

  if (bind_errno == EADDRINUSE) {
    return 0; // Port is in use
  }

  errno = bind_errno;
  return -1; // Other error
}

//===============================================================================
// HAPROXY AGENT-CHECK THREAD
//===============================================================================

/**
 * Agent-check thread function.
 * Listens on a TCP port and responds with "ready\n" or "drain\n" based on
 * whether the main thread is currently processing a request.
 *
 * HAProxy agent-check protocol:
 * - "ready" : Server is idle, can accept new requests
 * - "drain" : Server is busy, don't send new requests
 * - "down"  : Server is broken (we don't use this)
 * - "up"    : Server is available (we use "ready" instead for clarity)
 */
static void *agent_thread_func(void *arg) {
  (void)arg; // Unused

  LOG_DEBUG("Agent-check thread started");

  while (agent_running && running) {
    // Accept connection with timeout to allow clean shutdown
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(agent_socket_fd, &read_fds);

    int select_result = select(agent_socket_fd + 1, &read_fds, NULL, NULL, &tv);

    if (select_result < 0) {
      if (errno == EINTR) {
        continue; // Interrupted by signal, retry
      }
      LOG_ERROR("Agent select error: %s", strerror(errno));
      break;
    }

    if (select_result == 0) {
      // Timeout, check if we should keep running
      continue;
    }

    // Accept the connection
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd =
        accept(agent_socket_fd, (struct sockaddr *)&client_addr, &client_len);

    if (client_fd < 0) {
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;
      }
      LOG_ERROR("Agent accept error: %s", strerror(errno));
      continue;
    }

    // Determine status and respond
    const char *status;
    if (handlers_is_busy()) {
      status = "drain\n";
      LOG_DEBUG("Agent-check: responding 'drain' (busy)");
    } else {
      status = "ready\n";
      LOG_DEBUG("Agent-check: responding 'ready' (idle)");
    }

    // Send response (best effort, don't retry on partial write)
    ssize_t written = write(client_fd, status, strlen(status));
    if (written < 0) {
      LOG_DEBUG("Agent write error: %s", strerror(errno));
    }

    close(client_fd);
  }

  LOG_DEBUG("Agent-check thread exiting");
  return NULL;
}

/**
 * Start the HAProxy agent-check thread on the specified port.
 * Returns 0 on success, -1 on failure.
 */
static int start_agent_thread(const char *host, int port) {
  // Create socket
  agent_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (agent_socket_fd < 0) {
    LOG_ERROR("Failed to create agent socket: %s", strerror(errno));
    return -1;
  }

  // Set socket options
  int opt = 1;
  setsockopt(agent_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  // Bind to address
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((uint16_t)port);

  if (host && strlen(host) > 0 && strcmp(host, "0.0.0.0") != 0) {
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
      LOG_ERROR("Invalid agent bind address: %s", host);
      close(agent_socket_fd);
      agent_socket_fd = -1;
      return -1;
    }
  } else {
    addr.sin_addr.s_addr = INADDR_ANY;
  }

  if (bind(agent_socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    LOG_ERROR("Failed to bind agent socket to port %d: %s", port,
              strerror(errno));
    close(agent_socket_fd);
    agent_socket_fd = -1;
    return -1;
  }

  // Listen with small backlog (agent checks are quick)
  if (listen(agent_socket_fd, 5) < 0) {
    LOG_ERROR("Failed to listen on agent socket: %s", strerror(errno));
    close(agent_socket_fd);
    agent_socket_fd = -1;
    return -1;
  }

  // Start the thread
  agent_running = 1;
  if (pthread_create(&agent_thread_id, NULL, agent_thread_func, NULL) != 0) {
    LOG_ERROR("Failed to create agent thread: %s", strerror(errno));
    agent_running = 0;
    close(agent_socket_fd);
    agent_socket_fd = -1;
    return -1;
  }

  LOG_INFO("HAProxy agent-check listening on %s:%d",
           (host && strlen(host) > 0) ? host : "0.0.0.0", port);
  return 0;
}

/**
 * Stop the HAProxy agent-check thread.
 */
static void stop_agent_thread(void) {
  if (!agent_running) {
    return;
  }

  LOG_DEBUG("Stopping agent-check thread");
  agent_running = 0;

  // Close the socket to unblock accept()
  if (agent_socket_fd >= 0) {
    close(agent_socket_fd);
    agent_socket_fd = -1;
  }

  // Wait for thread to exit
  pthread_join(agent_thread_id, NULL);
  LOG_DEBUG("Agent-check thread stopped");
}

//===============================================================================
// SIGNAL HANDLERS
//===============================================================================

static void signal_handler(int signum) {
  if (signum == SIGTERM || signum == SIGINT) {
    LOG_INFO("Received signal %d, shutting down...", signum);
    running = 0;
  } else if (signum == SIGHUP) {
    LOG_INFO("Received SIGHUP, reopening log file...");
    // Log file reopen would be handled here if needed
  }
}

static void setup_signal_handlers(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGTERM, &sa, NULL) == -1) {
    LOG_WARN("Failed to set SIGTERM handler: %s", strerror(errno));
  }
  if (sigaction(SIGINT, &sa, NULL) == -1) {
    LOG_WARN("Failed to set SIGINT handler: %s", strerror(errno));
  }
  if (sigaction(SIGHUP, &sa, NULL) == -1) {
    LOG_WARN("Failed to set SIGHUP handler: %s", strerror(errno));
  }

  // Ignore SIGPIPE to prevent crashes on broken connections
  signal(SIGPIPE, SIG_IGN);
}

//===============================================================================
// DAEMONIZATION
//===============================================================================

static int daemonize(void) {
  // First fork
  pid_t pid = fork();
  if (pid < 0) {
    return -1;
  }
  if (pid > 0) {
    // Parent exits
    exit(0);
  }

  // Create new session
  if (setsid() < 0) {
    return -1;
  }

  // Second fork to ensure we can't acquire a controlling terminal
  pid = fork();
  if (pid < 0) {
    return -1;
  }
  if (pid > 0) {
    // First child exits
    exit(0);
  }

  // Set file mode mask
  umask(0);

  // Change to root directory to avoid blocking unmounts
  if (chdir("/") < 0) {
    return -1;
  }

  // Close standard file descriptors
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  // Redirect stdin/stdout/stderr to /dev/null
  int fd = open("/dev/null", O_RDWR);
  if (fd >= 0) {
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > STDERR_FILENO) {
      close(fd);
    }
  }

  return 0;
}

//===============================================================================
// LOGGING SETUP
//===============================================================================

/**
 * Convert daemon log level to c-logger LogLevel.
 */
static LogLevel convert_log_level(int daemon_level) {
  // daemon_config uses LOG_TRACE=0, LOG_DEBUG=1, etc. (same as c-logger)
  switch (daemon_level) {
  case 0:
    return LogLevel_TRACE;
  case 1:
    return LogLevel_DEBUG;
  case 2:
    return LogLevel_INFO;
  case 3:
    return LogLevel_WARN;
  case 4:
    return LogLevel_ERROR;
  case 5:
    return LogLevel_FATAL;
  default:
    return LogLevel_INFO;
  }
}

static int setup_logging(const daemon_config_t *config) {
  // Set log level
  logger_setLevel(convert_log_level(config->log_level));

  // If log file specified, use file logger
  if (strlen(config->log_file) > 0) {
    // Use 10MB max file size and 5 backup files
    if (!logger_initFileLogger(config->log_file, 10 * 1024 * 1024, 5)) {
      fprintf(stderr, "Error: Cannot open log file '%s'\n", config->log_file);
      return -1;
    }
    // Auto-flush every 100ms to ensure logs are written promptly
    logger_autoFlush(100);
  } else {
    // Log to stderr
    logger_initConsoleLogger(stderr);
  }

  return 0;
}

static void cleanup_logging(void) { logger_exitFileLogger(); }

//===============================================================================
// MAIN
//===============================================================================

int main(int argc, char *argv[]) {
  // Parse command line arguments
  daemon_config_t config = daemon_parse_arguments(argc, argv);

  // Show help if requested
  if (config.show_help) {
    daemon_print_help(argv[0]);
    return 0;
  }

  // Validate configuration
  if (!daemon_validate_config(&config)) {
    daemon_print_help(argv[0]);
    return 1;
  }

  // Setup logging
  if (setup_logging(&config) < 0) {
    return 1;
  }

  // Daemonize if requested (after logging setup)
  if (config.daemonize) {
    if (daemonize() < 0) {
      fprintf(stderr, "Error: Failed to daemonize: %s\n", strerror(errno));
      return 1;
    }
  }

  // Setup signal handlers
  setup_signal_handlers();

  // Initialize handlers (threat matrix, etc.)
  handlers_init();

  // Start HAProxy agent-check thread if configured
  if (config.agent_port > 0) {
    if (start_agent_thread(config.bind_host, config.agent_port) < 0) {
      fprintf(stderr, "Error: Failed to start agent-check thread on port %d\n",
              config.agent_port);
      cleanup_logging();
      return 1;
    }
  }

  // Check if port is available before trying to bind
  int port_check = check_port_available(config.bind_host, config.bind_port);
  if (port_check == 0) {
    LOG_FATAL("Port %d is already in use. Another process may be listening on "
              "this port.",
              config.bind_port);
    fprintf(stderr,
            "Error: Port %d is already in use. Another process may be "
            "listening on this port.\n",
            config.bind_port);
    cleanup_logging();
    return 1;
  } else if (port_check < 0 && errno != 0) {
    LOG_FATAL("Failed to check port availability: %s", strerror(errno));
    fprintf(stderr, "Error: Failed to check port availability: %s\n",
            strerror(errno));
    cleanup_logging();
    return 1;
  }

  // Create HTTP server
  struct http_server_s *server =
      http_server_init(config.bind_port, handle_request);
  if (!server) {
    LOG_FATAL("Failed to initialize HTTP server on port %d", config.bind_port);
    cleanup_logging();
    return 1;
  }

  LOG_INFO("gomoku-httpd v%s starting", DAEMON_VERSION);

  // Start listening using polling mode for graceful shutdown support
  int result;
  if (strlen(config.bind_host) > 0 &&
      strcmp(config.bind_host, "0.0.0.0") != 0) {
    result = http_server_listen_addr_poll(server, config.bind_host);
  } else {
    result = http_server_listen_poll(server);
  }

  if (result != 0) {
    int err = errno;
    const char *error_msg;

    switch (err) {
    case EADDRINUSE:
      error_msg = "Address already in use. Another process may be listening on "
                  "this port.";
      break;
    case EACCES:
      error_msg =
          "Permission denied. Try a port number above 1024 or run as root.";
      break;
    case EADDRNOTAVAIL:
      error_msg = "Address not available. Check the bind address is valid for "
                  "this host.";
      break;
    default:
      error_msg = strerror(err);
      break;
    }

    LOG_FATAL("Failed to bind to %s:%d: %s", config.bind_host, config.bind_port,
              error_msg);
    fprintf(stderr, "Error: Failed to bind to %s:%d: %s\n", config.bind_host,
            config.bind_port, error_msg);
    cleanup_logging();
    return 1;
  }

  LOG_INFO("Listening on %s:%d", config.bind_host, config.bind_port);

  // Event loop with graceful shutdown support
  while (running) {
    int poll_result = http_server_poll(server);
    if (poll_result < 0) {
      // Error or shutdown
      break;
    }
  }

  // Stop agent thread if running
  stop_agent_thread();

  LOG_INFO("Server stopped");
  cleanup_logging();

  return 0;
}
