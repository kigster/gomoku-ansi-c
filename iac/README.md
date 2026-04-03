# Infrastructure — Gomoku

Deployment configuration for Google Cloud Run with external PostgreSQL (Neon).

## Architecture

```
Internet → gomoku-api (Cloud Run, public) → gomoku-httpd (Cloud Run, internal)
               ↓
           Neon PostgreSQL (external)
```

- **gomoku-api** — FastAPI serving the React SPA as static files, plus auth, scoring, leaderboard, and game move proxy. Public-facing.
- **gomoku-httpd** — C game engine, stateless, single-threaded, concurrency=1. Internal only, auto-scales per demand.

Both services scale independently. Cloud Run handles TLS and load balancing automatically.

## Directory Structure

```
iac/
├── cloud_run/           # Terraform + deploy scripts for Cloud Run
│   ��── main.tf          # Two Cloud Run services + IAM
│   ├── variables.tf     # Project ID, region, images, secrets
│   ├── outputs.tf       # Service URLs
│   ├── deploy.sh        # First-time deploy (terraform init + apply)
│   ���── update.sh        # Update one or both services (build + push + gcloud)
��   └── README.md        # Full Cloud Run deployment guide
├─��� cloud_sql/           # Database schema and utilities
│   ├── setup.sql        # Schema: users, games, views, functions
│   ├── create-instance.sh  # Create Cloud SQL instance (if using GCP Postgres)
│   └── resolve-geo.sh   # Backfill IP geolocation data
├── local/               # Local development configs (used by bin/gctl)
│   ├── envoy/           # Envoy proxy config for local worker pool
│   └── templates/       # Config templates (envoy, nginx)
└── README.md            # This file
```

## Quick Start

```bash
# 1. Set up Neon database
psql "$NEON_DATABASE_URL" -f iac/cloud_sql/setup.sql

# 2. Set environment variables
export PROJECT_ID="your-gcp-project-id"
export TF_VAR_jwt_secret="$(openssl rand -base64 32)"
export TF_VAR_database_url="postgresql://user:pass@ep-xyz.neon.tech/gomoku?sslmode=require"

# 3. Build and deploy
just cr-init
```

See [cloud_run/README.md](cloud_run/README.md) for the full deployment guide, custom domain setup, scaling, and monitoring.

## Database Schema

The schema is in `cloud_sql/setup.sql`. Key tables:

- **users** — UUID PK, username, email, password hash, game counters
- **games** — UUID PK, player name, winner, depth, radius, score, game JSON, IP, geo
- **password_reset_tokens** — UUID PK, token, expiry

Score formula: `1000 * depth + 50 * radius + f(human_time_seconds)` where `f(x)` rewards fast wins and penalizes slow ones.

Views: `leaderboard` (best per player), `top_scores` (global top 100).

## Environment Variables

### FastAPI (`gomoku-api`)

| Variable | Source | Purpose |
|---|---|---|
| `DATABASE_URL` | `TF_VAR_database_url` | Neon PostgreSQL connection string |
| `GOMOKU_HTTPD_URL` | Terraform (from httpd service URL) | Internal URL to game engine |
| `JWT_SECRET` | `TF_VAR_jwt_secret` | JWT signing key |
| `CORS_ORIGINS` | Terraform variable | Allowed origins (default `["*"]`) |

### Game Engine (`gomoku-httpd`)

No environment variables — all config via CLI args (`-b 0.0.0.0:8787`).

## Local Development

Use `bin/gctl` instead of Cloud Run:

```bash
bin/gctl start              # Start nginx, envoy, gomoku-httpd workers, API
bin/gctl status             # Check what's running
bin/gctl stop               # Stop everything
```

See `bin/gctl -h` for full usage.
