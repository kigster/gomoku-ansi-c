# Gomoku API

FastAPI server that handles authentication, game proxy, scoring, leaderboard,
and serves the React frontend as static assets.

## Prerequisites

- Python 3.12+ managed by [uv](https://docs.astral.sh/uv/)
- PostgreSQL 16+ running locally
- [just](https://github.com/casey/just) command runner
- [direnv](https://direnv.net/) (loads `.envrc` which exports `.env` variables)

## Quick start

```bash
cp .env.example .env          # edit DATABASE_URL, JWT_SECRET
just install                   # install Python dependencies
just db-create                 # create gomoku + gomoku_test databases
just db-migrate-up             # run all Alembic migrations
just dev                       # start uvicorn with --reload (foreground)
```

The API is now at `http://localhost:8000`. Hit `GET /health` to verify.

## Environment variables

Configured in `.env` (gitignored) and loaded by direnv and pydantic-settings.

| Variable           | Default                                       | Description                                             |
| ------------------ | --------------------------------------------- | ------------------------------------------------------- |
| `DATABASE_URL`     | `postgresql://postgres@localhost:5432/gomoku` | PostgreSQL connection string                            |
| `JWT_SECRET`       | `change-me-in-production`                     | HMAC key for signing JWTs (see [Secrets](#secrets))     |
| `GOMOKU_HTTPD_URL` | `http://localhost:10000`                      | Upstream C game engine address                          |
| `ENVIRONMENT`      | `development`                                 | `development`, `production`, or `test`                  |
| `API_LISTEN_PORT`  | `8000`                                        | Uvicorn listen port                                     |
| `API_LISTEN_HOST`  | `0.0.0.0`                                     | Uvicorn bind address                                    |
| `UVICORN_WORKERS`  | `4`                                           | Worker count for `just start`                           |
| `LOGFIRE_TOKEN`    | _(unset)_                                     | Pydantic Logfire token; logfire is disabled when absent |
| `CORS_ORIGINS`     | `["*"]`                                       | Allowed CORS origins (JSON list)                        |

## Running the server

### Development

```bash
just dev
```

Runs uvicorn in the foreground with `--reload`. Logs go to stderr and
`logs/api.development.log`. Press Ctrl+C to stop.

### Production

```bash
just start          # background, 4 workers (configurable via UVICORN_WORKERS)
just stop           # SIGTERM then SIGKILL if needed
just restart        # stop + start
```

`just start` spawns multiple uvicorn workers (separate processes) for better
CPU utilization and process isolation. Logs append to
`logs/fastapi.{environment}.log`. A health check via `curl /health` runs
automatically after startup.

### Behind nginx

In production (and local dev via `dev.gomoku.games`), nginx sits in front:

- `/assets/*` -- served directly from `api/public/assets/` with long cache headers
- `/auth/*`, `/game/*`, `/leaderboard/*`, `/user/*`, `/health` -- proxied to uvicorn
- `/*` -- SPA fallback, serves `api/public/index.html`

The nginx config is generated from templates in `iac/local/templates/`.

## Static assets

The React frontend is built separately and copied into `api/public/`:

```bash
just install-frontend     # (from repo root) builds and copies dist/* to api/public/
```

FastAPI also has a built-in SPA fallback (in `app/main.py`) that serves files
from `public/` if the directory exists. This is used when running without nginx
(e.g., in Cloud Run where a single container serves both API and frontend).

## Database

### Setup

```bash
just db-create        # create gomoku + gomoku_test (idempotent)
just db-migrate-up    # run all Alembic migrations to head
```

Both commands operate on the primary and test databases.

### Migrations

Migrations live in `db/migrations/versions/` and use a timestamp-prefixed
naming scheme (`YYYYMMDD-HHMMSS-slug.py`).

```bash
just db-migrate-up              # upgrade both DBs to head
just db-migrate-up rev="+1"     # upgrade one step
just db-migrate-down            # downgrade one step (default)
just db-migrate-down rev="-2"   # downgrade two steps
just db-current                 # show current revision for both DBs
just db-history                 # show full migration history
just db-revision "add scores"   # create a new empty migration
just db-reset                   # drop, recreate, and migrate (destructive)
```

`just db-reset` requires that no active connections exist on the database.
Stop uvicorn first with `just stop`.

### Adopting an existing database

If the tables already exist but Alembic has never tracked them (no
`alembic_version` table), stamp the DB without running the DDL:

```bash
DATABASE_URL=... uv run alembic stamp head
```

## Secrets and authentication

### JWT_SECRET

This is the sole secret the API needs. It is used internally by the Python app
to HMAC-sign session tokens (`POST /auth/signup`, `POST /auth/login`) and
verify them on authenticated requests. No external service ever sees it.

**Rules:**

- Never commit it to the repo. `.env` is gitignored; `.env.example` has a
  placeholder.
- In production, set it via Google Secret Manager or Cloud Run env vars.
- Rotating the secret is safe -- the only side effect is that existing
  logged-in users must re-authenticate (their tokens fail verification).
- Generate one with: `openssl rand -hex 32`

### Request flow

1. Middleware decodes the JWT once and stashes a typed `CurrentSession` on
   `request.state.current_session`.
1. Route handlers access the session via the `get_session(request)` dependency
   -- no redundant decode.
1. `get_current_user` looks up the user in PostgreSQL using the `sub` claim
   from the session.

### Password hashing

Passwords are hashed with bcrypt via the `bcrypt` library. The hash is stored
in the `users.password_hash` column. The plaintext password never touches the
database or logs.

## CORS

CORS is configured via the `CORS_ORIGINS` environment variable, which accepts
a JSON list of allowed origins. The default is `["*"]` (allow all).

**When to restrict it:**

- In production behind nginx on the same domain, CORS is irrelevant -- the
  browser sees same-origin requests.
- If the frontend is served from a different domain (e.g., a CDN or separate
  Cloud Run service), set `CORS_ORIGINS` to the specific origin(s):
  `CORS_ORIGINS='["https://app.gomoku.games"]'`
- The `["*"]` default is fine for local development.

## Logging

Structured logging via structlog, routed through stdlib to two handlers:

- **stderr** -- colored output (when TTY) for development
- **`logs/api.{environment}.log`** -- plain text, rotating (5MB x 5 files)

Logfire (Pydantic's observability platform) is integrated but only activates
when `LOGFIRE_TOKEN` is set and `ENVIRONMENT != test`.

## Testing

```bash
just test                       # run all tests
just test -k test_auth          # filter by name
just test-coverage              # with branch coverage report
```

Tests use `gomoku_test` database (auto-created, auto-migrated by conftest).
The test suite sets `ENVIRONMENT=test`, which disables Logfire.

## Linting and type checking

```bash
just lint           # ruff check + format
just typecheck      # ty check
just ci             # lint + typecheck + format check
```

## API endpoints

| Method | Path                           | Auth | Description                       |
| ------ | ------------------------------ | ---- | --------------------------------- |
| `GET`  | `/health`                      | No   | Health check                      |
| `POST` | `/auth/signup`                 | No   | Create account, returns JWT       |
| `POST` | `/auth/login`                  | No   | Login, returns JWT                |
| `POST` | `/auth/password-reset`         | No   | Request password reset email      |
| `POST` | `/auth/password-reset/confirm` | No   | Confirm reset with token          |
| `POST` | `/game/play`                   | No   | Proxy move to gomoku-httpd engine |
| `POST` | `/game/start`                  | Yes  | Record game start                 |
| `POST` | `/game/save`                   | Yes  | Save completed game with scoring  |
| `GET`  | `/game/history`                | Yes  | User's game history               |
| `GET`  | `/game/{id}/json`              | Yes  | Download full game JSON           |
| `GET`  | `/user/me`                     | Yes  | Current user profile + stats      |
| `GET`  | `/leaderboard`                 | No   | Top scores                        |
