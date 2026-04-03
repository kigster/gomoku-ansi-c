# Infrastructure — Gomoku

Deployment configuration for Google Cloud Run and Cloud SQL.

## Architecture

```
Internet → Cloud Run (frontend/nginx) → Cloud Run (FastAPI) → Cloud Run (gomoku-httpd)
                                              ↓
                                         Cloud SQL (PostgreSQL 17)
```

- **gomoku-frontend** — nginx serving React SPA, proxies API calls to FastAPI. Public-facing.
- **gomoku-api** — FastAPI handling auth, game save, scoring, leaderboard. Proxies `/game/play` to gomoku-httpd. Internal only.
- **gomoku-httpd** — C game engine, stateless, single-threaded. Internal only.
- **Cloud SQL** — PostgreSQL 17 (`gomoku-db`), private IP, IAM auth.

All three Cloud Run services scale independently. The frontend and API handle
many concurrent connections; gomoku-httpd is limited to 1 request per instance
(single-threaded) and auto-scales to match demand.

## Directory Structure

```
iac/
├── cloud_run/           # Terraform + deploy scripts for Cloud Run
│   ├── main.tf          # Three Cloud Run services + IAM
│   ├── variables.tf     # Project ID, region, images, JWT secret
│   ├── outputs.tf       # Service URLs
│   ├── deploy.sh        # First-time deploy (terraform init + apply)
│   └── update.sh        # Update one or all services (build + push + gcloud)
├── cloud_sql/           # Database setup
│   ├── setup.sql        # Schema: users, games, views, functions
│   ├── create-instance.sh  # Create Cloud SQL instance
│   └── resolve-geo.sh   # Backfill IP geolocation data
├── local/               # Local development configs (used by bin/gctl)
│   ├── envoy/           # Envoy proxy config for local perf testing
│   └── templates/       # Config templates (envoy, nginx)
└── README.md            # This file
```

## First-Time Setup

### 1. Create Cloud SQL Instance

```bash
cd iac/cloud_sql
./create-instance.sh
```

This creates a PostgreSQL 17 instance (`gomoku-db`) with no public IP,
applies the schema, and connects it to the Cloud Run services.

### 2. Deploy to Cloud Run

```bash
# Set the JWT secret (generate a strong one for production)
export TF_VAR_jwt_secret="$(openssl rand -base64 32)"

# Build all Docker images and deploy
cd iac/cloud_run
./deploy.sh
```

Or from the project root:

```bash
export TF_VAR_jwt_secret="$(openssl rand -base64 32)"
just cr-init
```

### 3. Point DNS

Point `gomoku.games` (or `app.gomoku.games`) to the frontend service URL
shown in the deploy output.

## Updating Services

Update all services:

```bash
just cr-update
```

Update individual services:

```bash
cd iac/cloud_run
./update.sh frontend       # Frontend only
./update.sh api            # API only
./update.sh httpd          # Game engine only
./update.sh api frontend   # Multiple services
```

## Database Schema

The schema is in `cloud_sql/setup.sql`. Key tables:

- **users** — UUID PK, username, email, password hash, game counters
- **games** — UUID PK, player name, winner, depth, radius, score, game JSON, IP, geo
- **password_reset_tokens** — UUID PK, token, expiry

Score formula: `1000 * depth + 50 * radius + f(human_time_seconds)`
where `f(x)` rewards fast wins and penalizes slow ones.

Views: `leaderboard` (best per player), `top_scores` (global top 100).

## Environment Variables

### FastAPI (`gomoku-api`)

| Variable | Source | Purpose |
|---|---|---|
| `GOMOKU_HTTPD_URL` | Terraform (from httpd service URL) | Upstream game engine |
| `DB_SOCKET` | Terraform (Cloud SQL proxy path) | Database connection |
| `DB_NAME` | Terraform (`gomoku`) | Database name |
| `DB_USER` | Terraform (`postgres`) | Database user |
| `JWT_SECRET` | `TF_VAR_jwt_secret` | JWT signing key |

### Frontend (`gomoku-frontend`)

| Variable | Source | Purpose |
|---|---|---|
| `API_URL` | Terraform (from api service host:443) | nginx proxy target |

### Game Engine (`gomoku-httpd`)

No environment variables — all config via CLI args (`-b 0.0.0.0:8787`).

## Local Development

Use `bin/gctl` instead of Cloud Run:

```bash
bin/gctl start              # Start nginx, envoy, gomoku-httpd, API, frontend
bin/gctl start api frontend # Start specific components
bin/gctl status             # Check what's running
bin/gctl stop               # Stop everything
```

See `bin/gctl -h` for full usage.
