[![C99 Test Suite](https://github.com/kigster/gomoku-ansi-c/actions/workflows/c99.yml/badge.svg)](https://github.com/kigster/gomoku-ansi-c/actions/workflows/c99.yml) [![API Tests](https://github.com/kigster/gomoku-ansi-c/actions/workflows/api-test.yml/badge.svg)](https://github.com/kigster/gomoku-ansi-c/actions/workflows/api-test.yml) [![API Lint](https://github.com/kigster/gomoku-ansi-c/actions/workflows/api-lint.yml/badge.svg)](https://github.com/kigster/gomoku-ansi-c/actions/workflows/api-lint.yml)

# Gomoku (Five-in-a-Row)

A full-stack Gomoku game with a C-based AI engine, FastAPI backend, React frontend, and global leaderboard. Play it online at **[gomoku.games](https://app.gomoku.games)**.

<img src="doc/img/gomoku-web-version.png" width="700" border="1" style="border-radius: 10px"/>

## Architecture

```
Browser → React/Vite (frontend) → FastAPI (api) → gomoku-httpd (C engine)
                                       ↓
                                  PostgreSQL 17
```

| Component | Language | Purpose |
|---|---|---|
| **gomoku** | C99 | Terminal game with AI opponent |
| **gomoku-httpd** | C99 | Stateless HTTP game engine (single-threaded) |
| **api/** | Python/FastAPI | Auth, scoring, leaderboard, game proxy |
| **frontend/** | React/TypeScript | Web UI with auth, settings, board, leaderboard |

The React app talks exclusively to FastAPI. FastAPI proxies `/game/play` to gomoku-httpd and handles everything else (auth, game save, scoring, leaderboard) directly with PostgreSQL.

## Play Online

> **[https://app.gomoku.games](https://app.gomoku.games)**
>
> Running on Google Cloud Run. Creates containers on demand.

## Quick Start (Local)

### Terminal Game

```bash
make all
./gomoku                    # Human (X) vs AI (O), depth 3
./gomoku -d 5               # Harder AI
./gomoku -x ai -o ai -d 4:6 # Watch AI vs AI
./gomoku -h                 # All options
```

### Web Game

```bash
# Start everything
bin/gctl start

# Or start individual components
bin/gctl start gomoku api frontend
```

This starts nginx, envoy (load balancer for gomoku-httpd workers), FastAPI, and the Vite dev server. Open http://localhost:5173.

### Prerequisites

- C compiler (gcc/clang), Make, CMake
- Python 3.12+ with [uv](https://docs.astral.sh/uv/)
- Node.js 20+
- PostgreSQL 17+
- [just](https://github.com/casey/just) (optional, for convenience recipes)

## Game Rules

- Two players alternate placing stones on a 19x19 (or 15x15) board
- **X (Black) plays first** and has a slight advantage
- First to get **exactly 5 in a row** wins (horizontal, vertical, or diagonal)
- Six or more in a row does NOT count (overline rule)

## AI Engine

The AI uses **MiniMax with Alpha-Beta pruning**, enhanced with:

- **VCT (Victory by Continuous Threats)** — finds forced-win sequences up to 10 plies deep
- **Threat evaluation** — scores patterns (open fours, closed fours, compound threats, diamond forks)
- **Transposition table** — Zobrist hashing for position caching
- **Killer move heuristic** — prioritizes moves that caused beta cutoffs
- **Smart move ordering** — evaluates offense vs defense with configurable radius

### Threat Scoring

| Pattern | Score | Description |
|---|---|---|
| Five in a row | 1,000,000 | Win |
| Open four | 500,000 | Guaranteed win (two open ends) |
| Closed four | 100,000 | Must block (one open end) |
| Double four | 48,000 | Two fours — opponent can only block one |
| Four + three | 45,000 | Nearly winning compound |
| Double open three | 40,000 | Winning fork |
| Diamond fork (2x open two) | 25,000 | One move from double open three |
| Open three | 1,500 | Serious developing threat |

## Scoring & Leaderboard

When you win against the AI, your score is calculated as:

```
score = 1000 * depth + 50 * radius + f(time_seconds)
```

Where `f(x)` rewards fast wins (+1000 for instant, 0 at 2 minutes, -100 at 10 minutes). Higher depth = higher score tier — a depth-1 speed-run can never beat a depth-5 win.

Scores are stored in PostgreSQL with UUID primary keys, player geo-location (async IP lookup), and a global leaderboard.

## CLI Reference

```
gomoku [options]

  -d, --depth N        Search depth 1-10 (or N:M for asymmetric)
  -l, --level M        easy (2), medium (4), hard (6)
  -b, --board 15|19    Board size
  -r, --radius 1-5     Move generation radius
  -t, --timeout T      Seconds per move
  -x, --player-x TYPE  human or ai (default: human)
  -o, --player-o TYPE  human or ai (default: ai)
  -u, --undo           Enable undo (default: on, limit 5)
  -i, --hints          Highlight threatening patterns
  -j, --json FILE      Save game to JSON
  -p, --replay FILE    Replay a saved game
  -w, --wait SECS      Auto-advance replay delay
  -q, --quiet          Headless mode (AI vs AI, JSON output only)
  -s, --skip-welcome   Skip intro screen
  -h, --help           Show help
```

## Project Structure

```
├── src/gomoku/         # C game engine (ai, board, game, ui, cli)
├── src/net/            # C HTTP daemon (handlers, JSON API)
├── api/                # FastAPI service (auth, scoring, proxy)
│   ├── app/            # Python source
│   └── tests/          # 43 integration tests
├── frontend/           # React + TypeScript + Tailwind
│   └── src/            # Components, hooks, analytics
├── iac/                # Infrastructure (Cloud Run, Cloud SQL)
├── tests/              # C test suite (Google Test) + AI evals
├── bin/                # gctl cluster manager, helper scripts
├── Makefile            # C compilation (incremental builds)
└── justfile            # Everything else (docker, deploy, evals)
```

## Development

```bash
just --list             # See all available recipes
just build              # Build C binaries
just test               # Run C tests (game + daemon)
just docker-build-all   # Build all Docker images

# API
cd api && just ci       # Run API tests + lint
cd api && just serve    # Start FastAPI dev server

# Frontend
cd frontend && npm test # Run React tests
cd frontend && npm run dev # Start Vite dev server
```

## Deployment

Deployed to Google Cloud Run with Terraform. See [iac/README.md](iac/README.md).

```bash
export TF_VAR_jwt_secret="$(openssl rand -base64 32)"
just cr-init            # First-time deploy
just cr-update          # Update all services
```

## License & Copyright

MIT License. Copyright 2025-2026, Konstantin Gredeskoul.

## Acknowledgments

- Pattern recognition algorithms adapted from traditional Gomoku AI research
- [Claude](https://claude.ai) (Sonnet, Opus) — AI pair programming partner
- Google Test framework for C++ testing
