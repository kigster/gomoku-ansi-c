# Plan: Porting Gomoku from C to a Go-Based Web Service

This document outlines a comprehensive plan to migrate the single-threaded, terminal-based C Gomoku application to a concurrent, Go-based web service. The new service will expose a JSON API for gameplay and use WebSockets for real-time updates, with a primary focus on parallelizing the AI's move calculation.

## 1. Project Goal & High-Level Architecture

The primary goal is to create a scalable Gomoku backend that can serve multiple games simultaneously. The terminal UI will be replaced by a web API, enabling integration with any web or mobile front end. The core AI logic will be ported from C to Go and significantly enhanced by leveraging Go's native concurrency features (goroutines and channels) to parallelize the minimax search algorithm.

**Key Architectural Components:**

- **Language**: Go
- **Web Framework**: Standard `net/http` package for API endpoints, `gorilla/websocket` for real-time communication.
- **Concurrency**: Goroutines for parallelizing the AI search and handling concurrent API/WebSocket requests.
- **Data Format**: JSON for all API communication.
- **State Management**: In-memory storage for game states, managed concurrently. Each game will be an object identified by a unique ID.

## 2. Proposed Go Project Structure

A clean, idiomatic Go project structure will be established:

```
/gomoku-go/
├── go.mod
├── go.sum
├── cmd/
│   └── gomoku-server/
│       └── main.go         // Server entry point, HTTP router
├── internal/
│   ├── api/                // HTTP handlers and routing
│   │   ├── handlers.go
│   │   └── router.go
│   ├── game/               // Core game logic ported from C
│   │   ├── board.go
│   │   ├── game.go
│   │   └── types.go        // Go equivalents of C structs
│   ├── ai/                 // AI logic, including parallel minimax
│   │   ├── ai.go
│   │   ├── minimax.go
│   │   └── eval.go
│   └── websocket/          // WebSocket communication hub
│       └── hub.go
└── pkg/
    └── jsonmodels/         // Structs for API request/response bodies
```

## 3. Development Phases & Tasks

### Phase 1: Core Logic Porting (C to Idiomatic Go)

The first step is to create a solid, single-threaded foundation in Go by translating the essential game logic from C.

- **Task 1: Define Core Data Structures in Go.**
  - Translate C structs (`game_state_t`, `move_history_t`, etc.) into idiomatic Go structs (`Game`, `Board`, `Move`, `Player`).
  - The monolithic `game_state_t` will be broken down. A `Game` struct will contain the `Board`, `Players`, `MoveHistory`, and current state.
  - The board will be represented as a 2D slice: `[][]int`.

- **Task 2: Port Board and Rule Logic.**
  - Translate functions from `board.c` and `game.c` into Go methods.
  - Examples: `create_board` -> `NewBoard()`, `is_valid_move` -> `board.IsValidMove()`, `has_winner` -> `game.CheckWinner()`.
  - Ensure all game rules, win/draw conditions, and move validation logic are faithfully replicated.

- **Task 3: Port the Single-Threaded AI.**
  - Translate the evaluation functions (`evaluate_position`) and the minimax algorithm (`minimax_with_timeout`) from `ai.c` and `gomoku.c`.
  - At this stage, the goal is a direct port to validate correctness, without introducing parallelism yet.

### Phase 2: AI Parallelization

This is the core enhancement. The minimax algorithm is a perfect candidate for "embarrassingly parallel" computation at each level of the search tree.

- **Task 1: Design the Parallel Search.**
  - At any given node in the minimax search tree, the evaluation of each possible child move is independent.
  - The main `minimax` function will iterate through all valid moves. For each move, it will spawn a new **goroutine** to recursively call `minimax` on the resulting board state.

- **Task 2: Implement Goroutines and Channels.**
  - A channel will be created to aggregate the results (scores) from each goroutine.
  - The parent function will launch the goroutines and then block, waiting to receive a result from each one over the channel.
  - Alpha-beta pruning values must be shared safely between goroutines. A `sync.Mutex` can be used to protect access to the `alpha` and `beta` variables, ensuring that a better path found in one branch of the search can immediately prune other parallel branches.

- **Task 3: Implement Depth & Resource Management.**
  - The parallelization should be configurable, perhaps only kicking in after a certain depth to avoid the overhead of creating goroutines for trivial sub-problems.
  - Implement timeout mechanisms using `context.Context` to ensure that AI calculations do not run indefinitely.

### Phase 3: JSON Web API Implementation

With the game logic in place, build the web-facing API.

- **Task 1: Set up the HTTP Server.**
  - In `main.go`, initialize an HTTP server and a router (using `net/http`'s `ServeMux` or `gorilla/mux`).

- **Task 2: Implement Game Management.**
  - Create a thread-safe, in-memory repository to store active games (e.g., `map[string]*game.Game` protected by a `sync.RWMutex`).

- **Task 3: Implement API Endpoints.**
  - **`POST /game/new`**:
    - Accepts a JSON body with game settings (e.g., `board_size`, `player_one_type`, `player_two_type`, `ai_difficulty`).
    - Creates a new `Game` instance, generates a unique game ID, stores it, and returns the initial game state and ID as JSON.
  - **`POST /game/{id}/move`**:
    - Accepts a JSON body with the move details (e.g., `player`, `x`, `y`).
    - Validates the move against the current game state.
    - If valid, applies the move. If it's the AI's turn, this endpoint will trigger the parallel AI calculation **asynchronously** in a new goroutine and return an immediate `202 Accepted` response. The AI's move will be delivered later via WebSocket.
  - **`POST /game/{id}/move/undo`**:
    - Implements the undo logic by reverting the game state using the stored `MoveHistory`. This requires careful state management to correctly pop the last human and AI moves.

### Phase 4: Real-Time Communication with WebSockets

To provide a seamless user experience, the server will push updates to the client.

- **Task 1: Implement WebSocket Hub.**
  - Create a central "hub" that manages active WebSocket connections and maps them to game IDs.

- **Task 2: Create WebSocket Endpoint.**
  - **`GET /ws/game/{id}`**: An HTTP endpoint that upgrades the connection to a WebSocket.
  - Upon successful connection, the client's connection is registered with the hub for the specified game ID.

- **Task 3: Integrate with Game Logic.**
  - When the asynchronous AI move calculation (from Phase 3) is complete, the game logic will notify the WebSocket hub.
  - The hub will find all connections for that game ID and broadcast the AI's move and the new game state as a JSON message to all connected clients.

### Phase 5: Testing and Deployment

- **Task 1: Unit & Integration Testing.**
  - Write unit tests for the core game logic and AI evaluation functions.
  - Write integration tests for the API endpoints using the `net/http/httptest` package.
  - Crucially, use Go's **race detector** (`go test -race`) to identify and fix any concurrency issues in the parallel AI and game state management.

- **Task 2: Containerization.**
  - Create a `Dockerfile` to build a lightweight, distributable container image for the Go application.
  - This simplifies deployment and ensures a consistent runtime environment.

- **Task 3: Documentation.**
  - Create a simple `README.md` for the new Go project, detailing how to build and run the server, and documenting the API endpoints with example JSON payloads.

This plan provides a structured, phased approach to successfully porting and enhancing the Gomoku game, resulting in a modern, scalable, and highly performant web service.
