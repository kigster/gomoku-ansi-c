# Overview

The repository implements a terminal Gomoku game in ANSI C with an AI opponent based on MiniMax and alpha‑beta pruning. Code is organized into cohesive modules under src/, a test suite in tests/, and build scripts (Makefile, CMakeLists.txt) supporting both Make and CMake workflows.

## Core Structure

* Entry point (src/main.c) – parses CLI arguments, initializes game state, runs the main loop alternating human input and AI moves.

* Board utilities (src/board.c/h) – memory allocation, validation, and coordinate conversions with Unicode markers.

* Game logic (src/game.c/h) – central game_state_t structure, move history, undo, timers, transposition tables, killer moves, and other search optimizations.

* AI engine (src/ai.c/h) – move generation near existing stones, threat‑based move prioritization, MiniMax with alpha‑beta, incremental evaluation, timeout handling, killer moves, and transposition table lookups.

* Evaluation (src/gomoku.c/h) – threat recognition (e.g., five‑in‑a‑row, straight four), scoring matrices, incremental/full board evaluation, and win detection.

* User interface (src/ui.c/h) – ANSI terminal rendering, keyboard handling in raw mode, board/history/status display, and optional welcome screen.

* Command line interface (src/cli.c/h) – parses options such as depth, timeout, board size, undo, and help; validates configuration.

* Helpers (src/ansi.h) – ANSI escape sequences and color constants for the UI.

## Testing and Build

Tests (tests/gomoku_test.cpp) use Google Test to exercise board management, move validation, win detection, evaluation, AI, undo, and corner cases.

Setup: `./tests/setup` fetches Google Test into `tests/googletest/`

Makefile targets: 

* `make build`, 
* `make test`, 
* `make cmake-build`, 
* `make cmake-test`, etc.

CMake builds both game and tests, with tests/CMakeLists.txt pulling in local or system Google Test.

## Important Concepts for Newcomers

* `game_state_t` – the hub linking board state, move history, timers, and AI caches.
* Threat-based scoring – evaluation functions assign weights to patterns (five, four, three, broken, etc.).
* Search optimizations – incremental evaluation, cached “interesting” moves, transposition tables, killer moves, aspiration windows, and optional null-move pruning.
* ANSI UI – uses escape codes for colored board rendering and raw keyboard input.

## Suggested Next Steps

* Build & run locally: follow README/CLAUDE instructions to compile (make build or make cmake-build) and execute `./gomoku`

* Explore tests: run make test after running .`/tests/setup` to understand module contracts.

* Dive into AI algorithms: study MiniMax with alpha‑beta pruning, threat-space search, and transposition tables; experiment with evaluation heuristics or additional pruning methods.

* Extend gameplay: add features (e.g., save/load, different board sizes) or polish UI interactions.

* Refine performance: profile search routines, tweak move ordering, or explore parallel search.

* This modular design makes it straightforward to focus on one area (e.g., AI or UI) while understanding how it fits into the overall game loop.