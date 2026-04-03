[![C99 Test Suite](https://github.com/kigster/gomoku-ansi-c/actions/workflows/c99.yml/badge.svg)](https://github.com/kigster/gomoku-ansi-c/actions/workflows/c99.yml) [![API Tests](https://github.com/kigster/gomoku-ansi-c/actions/workflows/api-test.yml/badge.svg)](https://github.com/kigster/gomoku-ansi-c/actions/workflows/api-test.yml) [![API Lint](https://github.com/kigster/gomoku-ansi-c/actions/workflows/api-lint.yml/badge.svg)](https://github.com/kigster/gomoku-ansi-c/actions/workflows/api-lint.yml) [![Frontend Tests](https://github.com/kigster/gomoku-ansi-c/actions/workflows/frontend.yml/badge.svg)](https://github.com/kigster/gomoku-ansi-c/actions/workflows/frontend.yml)

# Gomoku (Five-in-a-Row)

A full-stack Gomoku game with a C-based AI engine, FastAPI backend, React frontend, and global leaderboard. Play it online at **[gomoku.games](https://app.gomoku.games)**.

<img src="doc/img/gomoku-web-version.png" width="700" border="1" style="border-radius: 10px"/>

## Architecture


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
├── gomoku-c/               # C game engine + HTTP daemon
│   ├── src/gomoku/         #   AI, board, game, UI, CLI
│   ├── src/net/            #   Stateless HTTP daemon (JSON API)
│   ├── tests/              #   Google Test suite + AI evals
│   ├── Makefile            #   Incremental C builds
│   └── Dockerfile          #   gomoku-httpd container
├── api/                    # FastAPI service
│   ├── app/                #   Auth, scoring, leaderboard, game proxy
│   ├── public/             #   Frontend static assets (built by justfile)
│   ├── tests/              #   Integration tests
│   └── Dockerfile          #   API container (includes frontend)
├── frontend/               # React + TypeScript + Tailwind
│   └── src/                #   Components, hooks, analytics
├── iac/                    # Infrastructure (Cloud Run, Cloud SQL, nginx)
├── bin/                    # gctl cluster manager, helper scripts
├── schema-validator/       # Ruby JSON schema validator
└── justfile                # Monorepo orchestration
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

## Application Configuration

The FastAPI server (`api/`) is configured via environment variables. Place them in `api/.env` for local development or set them in your Cloud Run / container environment.

### Database

| Variable | Default | Description |
|---|---|---|
| `DATABASE_URL` | *(none)* | Full PostgreSQL DSN, e.g. `postgresql://user:pass@host/gomoku`. Takes precedence over individual `DB_*` vars. |
| `DB_SOCKET` | *(none)* | Unix socket path for Cloud SQL Proxy, e.g. `/cloudsql/project:region:instance`. |
| `DB_NAME` | `gomoku` | Database name. |
| `DB_USER` | `postgres` | Database user. |
| `DB_PASSWORD` | *(none)* | Database password. Omit for local peer/trust auth. |

> **DSN resolution order**: `DATABASE_URL` if set; otherwise socket-based DSN if `DB_SOCKET` is set; otherwise `postgresql://{DB_USER}@localhost/{DB_NAME}`.

### Game Engine

| Variable | Default | Description |
|---|---|---|
| `GOMOKU_HTTPD_URL` | `http://localhost:8787` | Base URL of the upstream `gomoku-httpd` C engine. With envoy load-balancing, set to `http://localhost:10000`. In Cloud Run, set to the internal service URL. |

### Authentication (JWT)

| Variable | Default | Description |
|---|---|---|
| `JWT_SECRET` | `change-me-in-production` | HMAC signing key for JWT tokens. **Must** be changed in production. Generate with `openssl rand -base64 32`. |
| `JWT_ALGORITHM` | `HS256` | JWT signing algorithm. |
| `JWT_EXPIRE_MINUTES` | `1440` | Token lifetime in minutes (default 24 hours). |

### CORS

| Variable | Default | Description |
|---|---|---|
| `CORS_ORIGINS` | `["*"]` | JSON array of allowed origins. For production, restrict to your domain: `["https://app.gomoku.games"]`. |

### Email

| Variable | Default | Description |
|---|---|---|
| `EMAIL_PROVIDER` | `stdout` | `stdout` (logs to console) or `sendgrid`. |
| `EMAIL_FROM` | `noreply@gomoku.games` | Sender address for password-reset emails. |
| `SENDGRID_API_KEY` | *(none)* | Required when `EMAIL_PROVIDER=sendgrid`. |

### Runtime

| Variable | Default | Description |
|---|---|---|
| `PORT` | `8000` | Listening port (set automatically by Cloud Run). |

### Example `.env` (Local Development)

```env
DATABASE_URL=postgresql://postgres@localhost/gomoku
GOMOKU_HTTPD_URL=http://localhost:10000
JWT_SECRET=local-dev-secret-not-for-prod
CORS_ORIGINS=["http://localhost:5173","http://localhost:3000"]
EMAIL_PROVIDER=stdout
```

### Example Environment (Cloud Run)

```env
DB_SOCKET=/cloudsql/fine-booking-486503-k7:us-central1:gomoku
DB_PASSWORD=<secret>
GOMOKU_HTTPD_URL=https://gomoku-httpd-hdnatxbb3a-uc.a.run.app
JWT_SECRET=<generated-secret>
CORS_ORIGINS=["https://app.gomoku.games"]
EMAIL_PROVIDER=sendgrid
SENDGRID_API_KEY=<key>
```

## System Architecture

```mermaid
graph TB
    subgraph Client
        Browser["Browser<br/>(React + TypeScript)"]
    end

    subgraph "Cloud Run / Local"
        subgraph "FastAPI Service (api/)"
            ASGI["Uvicorn ASGI Server<br/>:8000"]
            MW["Middleware Stack<br/>CORS · Request Logging · Client IP"]
            AuthRouter["/auth/*<br/>signup · login · password-reset"]
            GameRouter["/game/*<br/>play · start · save"]
            LeaderRouter["/leaderboard/*"]
            UserRouter["/user/*"]
            Static["Static File Server<br/>api/public/ (SPA)"]
        end

        subgraph "Game Engine Cluster"
            Envoy["Envoy Proxy<br/>:10000<br/>(round-robin LB)"]
            HTTPD1["gomoku-httpd<br/>:8787"]
            HTTPD2["gomoku-httpd<br/>:8788"]
            HTTPD3["gomoku-httpd<br/>:8789"]
        end

        PG[("PostgreSQL<br/>gomoku database")]
    end

    Browser -->|"HTTPS"| ASGI
    ASGI --> MW --> AuthRouter & GameRouter & LeaderRouter & UserRouter & Static

    GameRouter -->|"POST /gomoku/play<br/>(httpx proxy)"| Envoy
    Envoy --> HTTPD1 & HTTPD2 & HTTPD3

    AuthRouter -->|"asyncpg"| PG
    GameRouter -->|"save score"| PG
    LeaderRouter -->|"query"| PG
    UserRouter -->|"query"| PG

    style Browser fill:#4A90D9,color:#fff
    style ASGI fill:#2D6A4F,color:#fff
    style Envoy fill:#E76F51,color:#fff
    style PG fill:#7B68EE,color:#fff
    style HTTPD1 fill:#D4A574,color:#000
    style HTTPD2 fill:#D4A574,color:#000
    style HTTPD3 fill:#D4A574,color:#000
```

### Request Flow

```mermaid
sequenceDiagram
    participant B as Browser
    participant F as FastAPI
    participant E as Envoy
    participant G as gomoku-httpd
    participant DB as PostgreSQL

    Note over B,DB: Game Play (authenticated)

    B->>F: POST /game/start
    F->>DB: UPDATE users SET games_started += 1
    F-->>B: 200 OK

    B->>F: POST /game/play {board, moves, settings}
    F->>E: POST /gomoku/play (proxy)
    E->>G: round-robin to worker
    G-->>E: {moves: [..., ai_move], winner}
    E-->>F: response
    F-->>B: AI move + board state

    Note over B,F: Repeat until winner != "none"

    B->>F: POST /game/save {full game JSON}
    F->>F: Calculate score<br/>1000×depth + 50×radius + f(time)
    F->>DB: INSERT INTO games (...)
    F->>DB: UPDATE users SET games_finished += 1
    F-->>B: {game_id, score, rating}

    Note over B,DB: Leaderboard

    B->>F: GET /leaderboard
    F->>DB: SELECT top scores
    F-->>B: [{username, score, depth, ...}]
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
