[![C99 Test Suite](https://github.com/kigster/gomoku-ansi-c/actions/workflows/c99.yml/badge.svg)](https://github.com/kigster/gomoku-ansi-c/actions/workflows/c99.yml) [![API Tests](https://github.com/kigster/gomoku-ansi-c/actions/workflows/api-test.yml/badge.svg)](https://github.com/kigster/gomoku-ansi-c/actions/workflows/api-test.yml) [![API Lint](https://github.com/kigster/gomoku-ansi-c/actions/workflows/api-lint.yml/badge.svg)](https://github.com/kigster/gomoku-ansi-c/actions/workflows/api-lint.yml) [![Frontend Tests](https://github.com/kigster/gomoku-ansi-c/actions/workflows/frontend.yml/badge.svg)](https://github.com/kigster/gomoku-ansi-c/actions/workflows/frontend.yml)

# Gomoku (Five-in-a-Row)

A full-stack Gomoku game: C99 AI engine, FastAPI backend, React frontend, global leaderboard. Play online at **[gomoku.games](https://app.gomoku.games)**.

<img src="doc/img/gomoku-web-version.png" width="700" border="1" style="border-radius: 10px"/>

## Build the Terminal Game

The terminal game has zero dependencies beyond a C compiler and Make.

```bash
# Build
just build-game          # or: make -C gomoku-c all install

# Play
bin/gomoku               # Human (X) vs AI (O), depth 3
bin/gomoku -d 5          # Harder AI
bin/gomoku -x ai -o ai   # Watch AI vs AI
bin/gomoku -h            # All options
```

### CLI Reference

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
  -j, --json FILE      Save game to JSON
  -p, --replay FILE    Replay a saved game
  -q, --quiet          Headless mode (AI vs AI, JSON output only)
  -h, --help           Show help
```

## Local Development

### Prerequisites

- C compiler (gcc/clang), Make
- Python 3.12+ with [uv](https://docs.astral.sh/uv/)
- Node.js 20+
- PostgreSQL 17+
- [just](https://github.com/casey/just)

### Start the Full Stack

```bash
bin/gctl setup           # One-time: install deps, create log files, SSL certs
bin/gctl start           # Start nginx, envoy, gomoku-httpd workers, FastAPI
```

Open https://dev.gomoku.games (local SSL via mkcert).

### gctl Commands

```bash
bin/gctl start [-w N]       # Start cluster (default 10 workers)
bin/gctl stop               # Stop everything
bin/gctl restart            # Restart all components
bin/gctl status             # Show running processes
bin/gctl ps                 # Process table (PID, CPU, MEM)
bin/gctl start nginx api    # Start individual components
bin/gctl observe btop       # Launch monitoring tool
```

Components: `nginx`, `envoy`, `gomoku` (httpd workers), `api` (FastAPI), `frontend` (Vite dev server).

### Architecture

```mermaid
graph TB
    subgraph Client
        Browser["Browser<br/>(React + TypeScript)"]
    end

    subgraph "Local Dev / Cloud Run"
        subgraph "FastAPI (:8000)"
            API["Auth, Scoring, Leaderboard<br/>Game Proxy, Static SPA"]
        end

        subgraph "Game Engine"
            Envoy["Envoy :10000<br/>(round-robin LB)"]
            W1["gomoku-httpd :8787"]
            W2["gomoku-httpd :8788"]
            WN["gomoku-httpd :878N"]
        end

        PG[("PostgreSQL")]
    end

    Browser -->|"HTTPS"| API
    API -->|"POST /gomoku/play"| Envoy
    Envoy --> W1 & W2 & WN
    API -->|"asyncpg"| PG

    style Browser fill:#4A90D9,color:#fff
    style API fill:#2D6A4F,color:#fff
    style Envoy fill:#E76F51,color:#fff
    style PG fill:#7B68EE,color:#fff
```

### justfile Recipes

```bash
just --list             # See all recipes
just build-game         # Build terminal game only
just build              # Build everything (C + frontend + API assets)
just test               # Run C tests
just test-api           # Run API tests (43 tests)
just test-frontend      # Run frontend tests (27 tests)
just docker-build-all   # Build all Docker images
```

## Deployment

### Option 1: Google Cloud Run (Recommended)

Serverless, scales to zero, cheapest for low traffic. Two containers: `gomoku-api` (FastAPI + React SPA) and `gomoku-httpd` (C engine).

```bash
export TF_VAR_jwt_secret="$(openssl rand -base64 32)"
just cr-init            # First-time: Terraform + deploy
just cr-update          # Subsequent updates
```

Requires: GCP project with Cloud Run + Cloud SQL enabled, `gcloud` CLI authenticated.

See [iac/README.md](iac/README.md) for full setup, DNS, and environment variables.

### Option 2: Any VPS with Docker Compose

Run on a $5/mo VPS (DigitalOcean, Hetzner, Fly.io).

```bash
just docker-build-all
docker compose up -d    # if you write a compose file
```

Minimum setup: PostgreSQL, two containers (`gomoku-api:latest`, `gomoku-httpd:latest`), a reverse proxy (nginx/Caddy) for TLS.

### Option 3: GKE / Kubernetes

Full Kubernetes with Envoy gateway, cert-manager, HPA autoscaling. More complex, better for high traffic.

```bash
bin/gcp-create-cluster setup
bin/gcp-create-cluster deploy
```

See [doc/DEPLOYMENT.md](doc/DEPLOYMENT.md) for the full guide comparing all options.

## Configuration

The FastAPI server reads environment variables from `api/.env`:

| Variable | Default | Purpose |
|---|---|---|
| `DATABASE_URL` | *(required)* | PostgreSQL DSN |
| `GOMOKU_HTTPD_URL` | `http://localhost:8787` | Upstream game engine (or envoy at `:10000`) |
| `JWT_SECRET` | `change-me-in-production` | HMAC signing key |
| `CORS_ORIGINS` | `["*"]` | Allowed origins (JSON array) |
| `EMAIL_PROVIDER` | `stdout` | `stdout` or `sendgrid` |

See the full configuration reference in the [Application Configuration](#application-configuration) section below.

## Project Structure

```
gomoku-c/               C game engine + HTTP daemon
  src/gomoku/             AI, board, game, UI, CLI
  src/net/                Stateless HTTP daemon (JSON API)
  tests/                  Google Test suite + AI evals
api/                    FastAPI service
  app/                    Auth, scoring, leaderboard, game proxy
  public/                 Frontend assets (built by justfile)
  tests/                  43 integration tests
frontend/               React + TypeScript + Tailwind
iac/                    Infrastructure (Cloud Run, Cloud SQL, nginx, envoy)
bin/                    gctl cluster manager, helper scripts
doc/                    Technical documentation
justfile                Monorepo orchestration
```

## Documentation

| Document | Description |
|---|---|
| [doc/DEPLOYMENT.md](doc/DEPLOYMENT.md) | Local cluster, Cloud Run, and GKE deployment |
| [doc/DEVELOPER.md](doc/DEVELOPER.md) | C engine technical overview and architecture |
| [doc/AI-ENGINE.md](doc/AI-ENGINE.md) | AI algorithm analysis, threat scoring, known issues |
| [doc/HTTPD.md](doc/HTTPD.md) | HTTP daemon API reference and cluster setup |
| [doc/GAME-RULES.md](doc/GAME-RULES.md) | Gomoku/Renju rules and variant support proposal |
| [doc/DTRACE.md](doc/DTRACE.md) | DTrace investigation of CPU busy-spin fix |
| [iac/README.md](iac/README.md) | Cloud Run infrastructure and Terraform |
| [frontend/CLAUDE.md](frontend/CLAUDE.md) | Frontend architecture and API endpoints |

## Application Configuration

The FastAPI server (`api/`) is configured via environment variables. Place them in `api/.env` for local development or set them in your Cloud Run / container environment.

### Database

| Variable | Default | Description |
|---|---|---|
| `DATABASE_URL` | *(none)* | Full PostgreSQL DSN, e.g. `postgresql://user:pass@host/gomoku`. Takes precedence over `DB_*` vars. |
| `DB_SOCKET` | *(none)* | Unix socket path for Cloud SQL Proxy. |
| `DB_NAME` | `gomoku` | Database name. |
| `DB_USER` | `postgres` | Database user. |
| `DB_PASSWORD` | *(none)* | Database password. |

### Game Engine

| Variable | Default | Description |
|---|---|---|
| `GOMOKU_HTTPD_URL` | `http://localhost:8787` | Upstream game engine. With envoy: `http://localhost:10000`. |

### Authentication (JWT)

| Variable | Default | Description |
|---|---|---|
| `JWT_SECRET` | `change-me-in-production` | HMAC signing key. Generate: `openssl rand -base64 32`. |
| `JWT_ALGORITHM` | `HS256` | JWT signing algorithm. |
| `JWT_EXPIRE_MINUTES` | `1440` | Token lifetime (default 24h). |

### CORS

| Variable | Default | Description |
|---|---|---|
| `CORS_ORIGINS` | `["*"]` | JSON array of allowed origins. |

### Email

| Variable | Default | Description |
|---|---|---|
| `EMAIL_PROVIDER` | `stdout` | `stdout` or `sendgrid`. |
| `EMAIL_FROM` | `noreply@gomoku.games` | Sender address. |
| `SENDGRID_API_KEY` | *(none)* | Required when `EMAIL_PROVIDER=sendgrid`. |

### Example `.env` (Local)

```env
DATABASE_URL=postgresql://postgres@localhost/gomoku
GOMOKU_HTTPD_URL=http://localhost:10000
JWT_SECRET=local-dev-secret-not-for-prod
CORS_ORIGINS=["http://localhost:5173"]
```

## License

MIT License. Copyright 2025-2026, Konstantin Gredeskoul.

## Acknowledgments

- [Claude](https://claude.ai) (Sonnet, Opus) -- AI pair programming partner
- Google Test framework for C++ testing
- Pattern recognition adapted from traditional Gomoku AI research
