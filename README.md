[![C99 Test Suite](https://github.com/kigster/gomoku-ansi-c/actions/workflows/c99.yml/badge.svg)](https://github.com/kigster/gomoku-ansi-c/actions/workflows/c99.yml) [![API Tests](https://github.com/kigster/gomoku-ansi-c/actions/workflows/api-test.yml/badge.svg)](https://github.com/kigster/gomoku-ansi-c/actions/workflows/api-test.yml) [![API Lint](https://github.com/kigster/gomoku-ansi-c/actions/workflows/api-lint.yml/badge.svg)](https://github.com/kigster/gomoku-ansi-c/actions/workflows/api-lint.yml) [![Frontend Tests](https://github.com/kigster/gomoku-ansi-c/actions/workflows/frontend.yml/badge.svg)](https://github.com/kigster/gomoku-ansi-c/actions/workflows/frontend.yml)

# Gomoku (Five-in-a-Row)

A full-stack Gomoku game: C99 AI engine, FastAPI backend, React frontend, global leaderboard. Play online at **[gomoku.games](https://app.gomoku.games)**.

<img src="doc/img/gomoku-web-version.png" width="700" border="1" style="border-radius: 10px"/>

## 1. Build and Play the Terminal Game

The terminal game is a standalone C99 binary with **zero runtime dependencies** — just a C compiler and Make.

### Build

```bash
# Using just (recommended)
just build-game

# Or directly with Make
make -C gomoku-c all install
```

This compiles three binaries into `bin/`:

| Binary | Purpose |
|---|---|
| `gomoku` | Interactive terminal game (ANSI color, arrow-key input) |
| `gomoku-httpd` | Stateless HTTP daemon for networked play |
| `gomoku-http-client` | CLI client for testing `gomoku-httpd` |

### Play

```bash
bin/gomoku                                    # Human (X) vs AI (O), depth 3
bin/gomoku -d 5                               # Harder AI (depth 5)
bin/gomoku -l hard                            # Same as -d 6
bin/gomoku -x ai -o ai -d 3:5                # AI vs AI, asymmetric depths
bin/gomoku -x ai -o ai -d 4 -q -j game.json  # Headless AI game, save to JSON
bin/gomoku -p game.json -w 0.5                # Replay saved game, 0.5s per move
bin/gomoku -b 19 -r 4 -t 60                  # 19x19 board, radius 4, 60s timeout
bin/gomoku -i                                 # Show threat hints (blink highlights)
```

### CLI Reference

```text
gomoku [options]

Gameplay:
  -b, --board 15|19    Board size (default: 15)
  -x, --player-x TYPE  human or ai (default: human)
  -o, --player-o TYPE  human or ai (default: ai)
  -u, --undo           Enable undo (default: on)
  -U, --undo-limit N   Max undo moves per game (default: 5, 0 = unlimited)
  -s, --skip-welcome   Skip the welcome screen
  -i, --hints          Highlight threatening patterns with blink
  -t, --timeout T      Seconds per move (AI picks best so far; human forfeits)

AI:
  -d, --depth N        Search depth 1-10 (or N:M for asymmetric)
  -l, --level M        easy (2), medium (4), hard (6)
  -r, --radius 1-5     Move generation radius (default: 3)

Recording:
  -j, --json FILE      Save game to JSON
  -p, --replay FILE    Replay a saved game
  -w, --wait SECS      Auto-advance replay (default: wait for keypress)
  -q, --quiet          Headless mode (AI vs AI, JSON output only)
  -h, --help           Show help
```

### AI Evaluations

```bash
just evals              # Run tactical tests + depth tournament
just eval-tactical      # Tactical position tests only
just eval-tournament    # AI vs AI depth tournament (depths 2,3,4)
just evals-ruby         # Ruby tournament against httpd cluster via envoy
```

See [doc/AI-ENGINE.md](doc/AI-ENGINE.md) for algorithm details and threat scoring.

## 2. Run the Networked Cluster Locally

The full stack runs on your dev machine: nginx for TLS, envoy for load balancing across a pool of `gomoku-httpd` workers, FastAPI for auth/scoring/leaderboard, and a Vite dev server for the React frontend.

### Prerequisites

| Dependency | Version | Purpose |
|---|---|---|
| C compiler (gcc/clang) | any | Build game engine |
| Make | any | Build system |
| [just](https://github.com/casey/just) | 1.0+ | Monorepo task runner |
| Python | 3.12+ | FastAPI backend |
| [uv](https://docs.astral.sh/uv/) | latest | Python package/venv manager |
| Node.js | 20+ | React frontend |
| PostgreSQL | 17+ | Leaderboard, user accounts, game history |
| [direnv](https://direnv.net/) | optional | Auto-loads `bin/` into `$PATH` |

### One-Time Setup

```bash
bin/gctl setup  # Installs deps, creates log files, generates local SSL certs (mkcert)
```

This runs four sub-setup steps: installs packages (`clang-format`, `shfmt`, `btop`, etc.), creates log files under `/var/log/`, generates SSL certs for `dev.gomoku.games`, and configures envoy/nginx templates.

> [!NOTE]
> 
> Please add dev.gomoku.games to your `/etc/hosts` file mapped to 127.0.0.1

### Create the Database

```bash
psql -X -c "CREATE DATABASE gomoku"
psql -X -d gomoku -f iac/cloud_sql/setup.sql
```

Then create `api/.env`:

```env
DATABASE_URL=postgresql://postgres@localhost/gomoku
GOMOKU_HTTPD_URL=http://localhost:10000
JWT_SECRET=local-dev-secret-not-for-prod
CORS_ORIGINS=["http://localhost:5173","https://dev.gomoku.games"]
```

### Start the Cluster

```bash
bin/gctl start           # Start nginx + envoy + gomoku-httpd workers + FastAPI
bin/gctl start -w 4      # Start with 4 workers instead of default (one per CPU core)
```

Open **<https://dev.gomoku.games>** (local SSL via mkcert).

### `gctl` Command Reference

```bash
bin/gctl start [-w N]       # Start cluster (default: 1 worker per CPU core)
bin/gctl stop               # Stop everything
bin/gctl restart            # Restart all components
bin/gctl status             # Show running processes
bin/gctl ps                 # Process table (PID, PPID, CPU, MEM, ARGS)
bin/gctl start nginx api    # Start individual components
bin/gctl observe btop       # Launch monitoring (btop, htop, ctop, btm)
```

Components: `nginx`, `envoy`, `gomoku` (httpd workers), `api` (FastAPI), `frontend` (Vite dev server).

> **Tip:** Use `direnv` so that `bin/` is on your `$PATH` — then you can just type `gctl start`.

### Architecture

```mermaid
graph TB
    subgraph Client
        Browser["Browser<br/>(React + TypeScript)"]
    end

    subgraph "Local Dev Cluster"
        Nginx["nginx :443<br/>(TLS termination)"]

        subgraph "FastAPI (:8000)"
            API["Auth, Scoring, Leaderboard<br/>Game Proxy, Static SPA"]
        end

        subgraph "Game Engine Pool"
            Envoy["Envoy :10000<br/>(least-request LB)"]
            W1["gomoku-httpd :9500"]
            W2["gomoku-httpd :9501"]
            WN["gomoku-httpd :950N"]
        end

        PG[("PostgreSQL")]
    end

    Browser -->|"HTTPS"| Nginx
    Nginx --> API
    API -->|"POST /gomoku/play"| Envoy
    Envoy --> W1 & W2 & WN
    API -->|"asyncpg"| PG

    style Browser fill:#4A90D9,color:#fff
    style Nginx fill:#009639,color:#fff
    style API fill:#2D6A4F,color:#fff
    style Envoy fill:#E76F51,color:#fff
    style PG fill:#7B68EE,color:#fff
```

Each `gomoku-httpd` worker is single-threaded, so envoy distributes requests across the pool using least-request load balancing. See [doc/DEPLOYMENT.md](doc/DEPLOYMENT.md) for the full local cluster guide.

### justfile Recipes

```bash
just --list             # See all recipes
just build-game         # Build terminal game only
just build              # Build everything (C + frontend + API assets)
just test               # Run C tests + daemon tests + API tests + frontend tests
just test-api           # Run API tests (43 tests)
just test-frontend      # Run frontend tests (27 tests)
just docker-build-all   # Build all Docker images
just ci                 # Run all pre-commit checks (lefthook)
```

## 3. Deploy to Production

The application needs two containers and a PostgreSQL database. All three deployment options below assume you outsource PostgreSQL to a managed provider.

### Database: Use Neon (or any managed Postgres)

[Neon](https://neon.tech) offers a generous free tier with serverless Postgres. Alternatives: [Supabase](https://supabase.com), [Aiven](https://aiven.io), Google Cloud SQL, AWS RDS.

1. Create a Neon project and database named `gomoku`.
2. Run the schema migration:

   ```bash
   psql "$NEON_DATABASE_URL" -f iac/cloud_sql/setup.sql
   ```

3. Copy the connection string — you'll set it as `DATABASE_URL` below.

> The schema creates `users`, `games`, and `password_reset_tokens` tables plus leaderboard views. See [iac/README.md](iac/README.md) for details.

### Option A: Google Cloud Run (Recommended)

Serverless, scales to zero, cheapest for low/medium traffic. Managed by Terraform.

Two Cloud Run services:

- **gomoku-api** — FastAPI + React SPA (nginx), handles auth, scoring, leaderboard, and proxies game moves
- **gomoku-httpd** — C game engine, single-threaded, concurrency=1, auto-scales per demand

#### Prerequisites

- GCP project with billing enabled
- `gcloud` CLI authenticated (`gcloud auth login`)
- Terraform installed
- Docker with `buildx` (for cross-compiling `linux/amd64` on Apple Silicon)

#### First-Time Deploy

```bash
# Generate a JWT signing secret
export TF_VAR_jwt_secret="$(openssl rand -base64 32)"

# Set your Neon (or other) database URL
export TF_VAR_database_url="postgresql://user:pass@ep-xyz.us-east-2.aws.neon.tech/gomoku?sslmode=require"

# Build Docker images (linux/amd64) and deploy via Terraform
just cr-init
```

This runs `iac/cloud_run/deploy.sh`, which:

1. Initializes Terraform and enables Cloud Run + Artifact Registry APIs
2. Creates an Artifact Registry repository (`gomoku-repo`)
3. Builds both Docker images for `linux/amd64` and pushes them
4. Applies the full Terraform plan (Cloud Run services, IAM, networking)

#### Subsequent Updates

```bash
just cr-update           # Rebuild images, push, update Cloud Run services
```

Or update individual services:

```bash
cd iac/cloud_run
./update.sh httpd        # Game engine only
./update.sh api          # API + frontend only
./update.sh httpd api    # Both
```

#### DNS

Point your domain to the frontend service URL from the deploy output. Cloud Run handles TLS automatically.

See [iac/README.md](iac/README.md) for the full infrastructure reference, environment variables, and architecture diagram.

### Option B: AWS (ECS Fargate or App Runner)

If you prefer AWS, the Docker images work without modification.

#### With App Runner (simplest)

```bash
# Push images to ECR
aws ecr create-repository --repository-name gomoku-httpd
aws ecr create-repository --repository-name gomoku-api

just docker-build-all-amd64

# Tag and push (replace 123456789.dkr.ecr.us-east-1.amazonaws.com with your ECR URI)
docker tag gomoku-httpd:latest $ECR_URI/gomoku-httpd:latest
docker tag gomoku-api:latest $ECR_URI/gomoku-api:latest
docker push $ECR_URI/gomoku-httpd:latest
docker push $ECR_URI/gomoku-api:latest
```

Then create two App Runner services in the AWS console or via `aws apprunner create-service`, setting these environment variables on the `gomoku-api` service:

```env
DATABASE_URL=postgresql://user:pass@ep-xyz.us-east-2.aws.neon.tech/gomoku?sslmode=require
GOMOKU_HTTPD_URL=https://<httpd-app-runner-url>
JWT_SECRET=<your-generated-secret>
CORS_ORIGINS=["https://yourdomain.com"]
```

#### With ECS Fargate

For more control (custom VPC, ALB, autoscaling policies), define an ECS task definition with two containers and an ALB. The `gomoku-httpd` container should have `desiredCount` scaled based on CPU, since each instance handles one game move at a time.

### Option C: Any VPS with Docker Compose

Run on a $5/mo VPS (DigitalOcean, Hetzner, Fly.io).

```bash
just docker-build-all
docker compose up -d
```

Minimum setup: two containers (`gomoku-api:latest` on port 8000, `gomoku-httpd:latest` on port 8787), a reverse proxy (nginx/Caddy) for TLS. Set environment variables as shown in the [Configuration](#configuration) section.

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
