# Gomoku HTTP Daemon

A stateless HTTP server that provides a REST API for playing Gomoku games. The daemon accepts game state via JSON, makes an AI move, and returns the updated state.

## Table of Contents

- [Building](#building)
- [HTTP Daemon](#http-daemon)
  - [Usage](#usage)
  - [CLI Options](#cli-options)
  - [Examples](#daemon-examples)
- [HTTP API](#http-api)
  - [POST /gomoku/play](#post-gomokuplay)
  - [GET /health](#get-health)
  - [Error Responses](#error-responses)
- [JSON Game Format](#json-game-format)
- [Testing with curl](#testing-with-curl)
- [Test Client](#test-client)
  - [Usage](#test-client-usage)
  - [CLI Options](#test-client-options)
  - [Examples](#test-client-examples)
- [Logging](#logging)

---

## Building

Build both the daemon and test client:

```bash
make all                    # Build gomoku and gomoku-http-daemon
make gomoku-http-daemon     # Build only the HTTP daemon
make test-gomoku-http       # Build the test client
```

Clean build artifacts:

```bash
make clean
```

---

## HTTP Daemon

### Usage

```
gomoku-http-daemon -b <host:port> [options]
```

The daemon runs as a foreground process by default. Use `-d` to daemonize.

### CLI Options

| Flag | Long Form | Description | Default |
|------|-----------|-------------|---------|
| `-b` | `--bind` | Host and port to bind (required) | - |
| `-d` | `--daemonize` | Run as background daemon | No |
| `-l` | `--log-file` | Path to log file | stdout |
| `-L` | `--log-level` | Log level: TRACE, DEBUG, INFO, WARN, ERROR, FATAL | INFO |
| `-h` | `--help` | Show help message | - |

### Daemon Examples

Start daemon on localhost port 3000:

```bash
./gomoku-http-daemon -b 127.0.0.1:3000
```

Start daemon on all interfaces with debug logging:

```bash
./gomoku-http-daemon -b 0.0.0.0:8080 -L DEBUG
```

Run as background daemon with log file:

```bash
./gomoku-http-daemon -d -b 0.0.0.0:3000 -l /var/log/gomoku.log
```

Stop the daemon:

```bash
pkill gomoku-http-daemon
# or send SIGTERM
kill $(pgrep gomoku-http-daemon)
```

---

## HTTP API

### POST /gomoku/play

Make an AI move in a Gomoku game.

**Request:**
- Method: `POST`
- Content-Type: `application/json`
- Body: JSON game state (see [JSON Game Format](#json-game-format))

**Response:**
- Content-Type: `application/json`
- Body: Updated JSON game state with AI's move appended

**Behavior:**
1. Parses the JSON game state
2. Caps AI depth to 6 and radius to 4 (for performance)
3. If game already has a winner, returns unchanged
4. Determines which player AI should be (opposite of last move)
5. Computes best move using minimax with alpha-beta pruning
6. Appends move to history and updates board state
7. Checks for winner after move
8. Returns updated game state

### GET /health

Health check endpoint.

**Request:**
- Method: `GET`

**Response:**
```json
{
  "status": "ok",
  "version": "1.0.0",
  "uptime": "2h 15m 30s"
}
```

### Error Responses

| Status | Description |
|--------|-------------|
| 400 | Bad Request - Invalid JSON or missing required fields |
| 404 | Not Found - Unknown endpoint |
| 405 | Method Not Allowed - Wrong HTTP method |
| 500 | Internal Server Error - AI failed to compute move |

Error response format:
```json
{
  "error": "Error message describing the problem"
}
```

---

## JSON Game Format

The game state JSON format:

```json
{
  "X": {
    "player": "human",
    "time_ms": 0.0
  },
  "O": {
    "player": "AI",
    "depth": 2,
    "time_ms": 1.234
  },
  "board": 15,
  "radius": 2,
  "timeout": "none",
  "winner": "none",
  "board_state": [
    ". . . . . . . . . . . . . . .",
    ". . . . . . . . . . . . . . .",
    "..."
  ],
  "moves": [
    {
      "X (human)": [7, 7],
      "time_ms": 0.0
    },
    {
      "O (AI)": [8, 8],
      "time_ms": 0.456
    }
  ]
}
```

### Field Descriptions

| Field | Type | Description |
|-------|------|-------------|
| `X` | object | Player X configuration |
| `X.player` | string | "human" or "AI" |
| `X.depth` | int | AI search depth (1-6, only if AI) |
| `X.time_ms` | float | Total time spent by this player |
| `O` | object | Player O configuration (same structure as X) |
| `board` | int | Board size: 15 or 19 |
| `radius` | int | AI search radius (1-4) |
| `timeout` | string | Move timeout: "none" or seconds |
| `winner` | string | "none", "X", "O", or "draw" |
| `board_state` | array | Visual board representation (optional, regenerated) |
| `moves` | array | Move history |

### Move Format

Each move in the `moves` array:

```json
{
  "X (human)": [x, y],
  "time_ms": 0.123
}
```

Or for AI moves:

```json
{
  "O (AI)": [x, y],
  "time_ms": 0.456
}
```

Coordinates are 0-indexed from the top-left corner.

---

## Testing with curl

### Start a New Game

Create a game where human plays X and AI plays O:

```bash
curl -X POST http://127.0.0.1:3000/gomoku/play \
  -H "Content-Type: application/json" \
  -d '{
    "X": {"player": "human", "time_ms": 0},
    "O": {"player": "AI", "depth": 2, "time_ms": 0},
    "board": 15,
    "radius": 2,
    "timeout": "none",
    "winner": "none",
    "board_state": [],
    "moves": [{"X (human)": [7, 7], "time_ms": 0}]
  }'
```

### Continue a Game

Take the response from the previous call, add your move to the `moves` array, and send it back:

```bash
# Save response to file
curl -X POST http://127.0.0.1:3000/gomoku/play \
  -H "Content-Type: application/json" \
  -d @game_state.json > updated_state.json
```

### Health Check

```bash
curl http://127.0.0.1:3000/health
```

Output:
```json
{"status":"ok","version":"1.0.0","uptime":"5m 30s"}
```

### Test Different AI Depths

Depth 1 (fastest, weakest):
```bash
curl -X POST http://127.0.0.1:3000/gomoku/play \
  -H "Content-Type: application/json" \
  -d '{"X":{"player":"human"},"O":{"player":"AI","depth":1},"board":15,"radius":2,"timeout":"none","winner":"none","board_state":[],"moves":[{"X (human)":[7,7],"time_ms":0}]}'
```

Depth 4 (slowest, strongest):
```bash
curl -X POST http://127.0.0.1:3000/gomoku/play \
  -H "Content-Type: application/json" \
  -d '{"X":{"player":"human"},"O":{"player":"AI","depth":4},"board":15,"radius":3,"timeout":"none","winner":"none","board_state":[],"moves":[{"X (human)":[7,7],"time_ms":0}]}'
```

---

## Test Client

An automated test client that plays complete games against the HTTP daemon.

### Test Client Usage

```
test-gomoku-http [options]
```

The client plays as X (human) using a simple spiral strategy while the server's AI plays as O.

### Test Client Options

| Flag | Long Form | Description | Default |
|------|-----------|-------------|---------|
| `-h` | `--host` | Server host | 127.0.0.1 |
| `-p` | `--port` | Server port | 3000 |
| `-d` | `--depth` | AI search depth (1-6) | 2 |
| `-r` | `--radius` | AI search radius (1-4) | 2 |
| `-b` | `--board` | Board size (15 or 19) | 15 |
| `-j` | `--json` | Save final game to JSON file | - |
| `-v` | `--verbose` | Show full game state after each move | No |
| | `--help` | Show help message | - |

### Test Client Examples

Play a game with default settings:

```bash
./test-gomoku-http -h 127.0.0.1 -p 3000
```

Play with stronger AI (depth 4):

```bash
./test-gomoku-http -p 3000 -d 4 -r 3
```

Play on 19x19 board and save result:

```bash
./test-gomoku-http -p 3000 -b 19 -j game_result.json
```

Play with verbose output to see full game state:

```bash
./test-gomoku-http -p 3000 -v
```

### Sample Output

```
Connecting to gomoku-http-daemon at 127.0.0.1:3000
Playing as X (human) against O (AI depth=2, radius=2, board=15)

Move 1: X plays [7, 7]
Move 2: O plays [8, 8]
Move 3: X plays [6, 6]
Move 4: O plays [7, 9]
Move 5: X plays [6, 7]
Move 6: O plays [6, 8]
Move 7: X plays [7, 6]
Move 8: O plays [5, 7]
Move 9: X plays [7, 8]
Move 10: O plays [5, 6]
Move 11: X plays [8, 6]
Move 12: O plays [4, 6]
Move 13: X plays [8, 7]

Final board:
  . . . . . . . . . . . . . . .
  . . . . . . . . . . . . . . .
  . . . . . . . . . . . . . . .
  . . . . . O . . . . . . . . .
  . . . . . . O . . . . . . . .
  . . . . . . O O . . . . . . .
  . . . . . . X X O . . . . . .
  . . . . . . X X X O . . . . .
  . . . . . . X X O . . . . . .
  . . . . . . . . . . . . . . .
  . . . . . . . . . . . . . . .
  . . . . . . . . . . . . . . .
  . . . . . . . . . . . . . . .
  . . . . . . . . . . . . . . .
  . . . . . . . . . . . . . . .

Game over: O (AI) wins!
Total moves: 13
Game saved to: game_result.json
```

---

## Logging

The daemon logs each HTTP request at INFO level with:

- **Client IP**: Source IP address
- **Path**: Request path
- **Status Code**: HTTP response status
- **Response Time**: Processing time in milliseconds

### Log Format

```
2026-01-29 18:47:51 INFO  src/net/handlers.c:80: 127.0.0.1 /gomoku/play 200 0.792ms
2026-01-29 18:47:51 INFO  src/net/handlers.c:80: 127.0.0.1 /health 200 0.037ms
```

### Log Levels

| Level | Description |
|-------|-------------|
| TRACE | Very detailed debugging information |
| DEBUG | Debugging information (request details, AI decisions) |
| INFO | Normal operation (request logs, game outcomes) |
| WARN | Warnings (invalid requests, capped parameters) |
| ERROR | Errors (failed operations) |
| FATAL | Fatal errors (startup failures) |

### Signal Handling

| Signal | Action |
|--------|--------|
| SIGTERM | Graceful shutdown |
| SIGINT | Graceful shutdown (Ctrl+C) |
| SIGHUP | Log file reopen (for log rotation) |
| SIGPIPE | Ignored (prevents crash on broken connections) |

---

## Architecture

```
Client                    Daemon
  |                         |
  |  POST /gomoku/play      |
  |  {game state JSON}      |
  |------------------------>|
  |                         |
  |                    Parse JSON
  |                    Restore game
  |                    Find AI move
  |                    Update state
  |                    Serialize JSON
  |                         |
  |  {updated game JSON}    |
  |<------------------------|
  |                         |
```

The daemon is completely stateless. All game state is passed in each request and returned in each response. This allows:

- Horizontal scaling (multiple daemon instances)
- Client-side game persistence
- Easy debugging (full state in each request)
- No session management required
