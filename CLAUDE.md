# Gomoku Project

Welcome to Gomoku Project.

Gomoku, also called "five in a row", is an abstract strategy board game. It is traditionally played with Go pieces (black and white stones) on a 15×15 Go board while in the past a 19×19 board was standard. Because pieces are typically not moved or removed from the board, gomoku may also be played as a paper-and-pencil game. The game is known in several countries under different names, like "crosses and naughts", etc.

## Gomoku Components

Currently, building the game with `make clean all` results in:

* a single binary `gomoku` that plays with the AI by the default, but accepts many CLI flags to adjust the difficulty.

* a single binary `gomoku-httpd` which listens on a HTTP port to POST to /gomoku/play and expect to receive a JSON response, schema for which is in the `doc` folder.

* A single binary `gomoku-http-test` (with it's own CLI) that connects to the port of `gomoku-httpd` (or several) and plays a game where the state is on the client's side, but the servers receive JSON representing a game state, they figure out how's next, and find the best move, returning the JSON with one additional move unless there is a win.

* We now have a web front-end that can talk to the `gomoku-httpd` daemon and make it play against itself.

* We also now have the cluster version that works locally in development (on a MacBook):

  * in development, first run `bin/gctl setup` to get everything installed and setup. The bash setup function is an aggregation function which calls four more specific setups.
  * in the development we should be starting the game cluster with `bin/gctl start` (starts envoy reverse proxy, nginx, gomoku-httpd).
  * or `bin/gctl start [ -p haproxy ]` to use haproxy instead
  * stopped with `gctl stop`
  * restarted with `gctl restart`
  * monitored with `gctl observe [ htop | btop | ctop | btm ]`
  * monitored with `gctl ps` — prints all the processes related to the cluster using a custom format ps sequence: PID, PPID, %CPU, %MEM, ARGS

### Current Deploy

The canonical deploy command is **`just deploy`** — it sources `.env` at repo root, runs Alembic migrations against the production database, builds the frontend + Docker images for `linux/amd64`, applies Terraform, and posts a deploy marker to Honeycomb. The actual logic lives in `bin/deploy`.

Required `.env` keys at repo root (deploy-time only — never read at runtime):

- `PRODUCTION_DATABASE_URL` — Neon pooled DSN
- `PRODUCTION_JWT_SECRET` — HMAC key (`just jwt-secret` to generate)
- `HONEYCOMB_INGEST_API_KEY` — runtime tracing
- `HONEYCOMB_CONFIG_API_KEY` — deploy markers
- `PROJECT_ID`, `REGION`

Legacy `just cr-init` and `just cr-update` still exist as escape hatches but skip migrations — prefer `just deploy`.

### Runtime Configuration

The FastAPI app loads `api/.env.{development,test,ci}[.local]` based on the `ENVIRONMENT` env var (default `development`). The `.local` overlays are gitignored for personal overrides (e.g., pointing local dev at Neon). Production runtime config arrives via Cloud Run env vars set by Terraform; no `.env` file is read in production.

### Tests

`just test-api` runs the API test suite in parallel across 4 workers via pytest-xdist (currently ~145 tests; multiplayer adds 56). Each worker gets its own `gomoku_test_gw{N}` database, dropped at session end. Sequential `just test` from `api/` also works for debugging.

### Multiplayer (human vs human)

The FastAPI server hosts a complete two-human game flow under
`/multiplayer/*` (see `api/app/routers/multiplayer.py`). The frontend's
`ChooseGameTypeModal` lets a logged-in user pick AI or Another Player —
the latter generates a 15-minute invite link (`/play/<6-char>`) the host
shares. Highlights a future maintainer should know:

- **No SQLAlchemy** — all DB access is asyncpg + raw SQL, with savepoints
  for the code-collision retry path.
- **Schema discriminator** — `games.game_type IN ('ai','multiplayer')` keeps
  the strict AI invariants (`depth>=1`, `radius>=1`, `total_moves>0`)
  while admitting `0/0/0` sentinels for multiplayer history rows.
- **Lazy expiry** — every read of a `waiting` game past its `expires_at`
  flips it to `cancelled`; no background sweeper is required for the
  modal flow.
- **Polling backoff** — both `useMultiplayerPolling` and
  `useMultiplayerHostPolling` geometrically back off after 304s and stop
  after a wall-clock cap (15 min for waiting, 8 h for in-progress).

Reference docs: `doc/human-vs-human-plan.md` (architecture & API),
`doc/multiplayer-modal-plan.md` (UX), `doc/multiplayer-bugs.md`
(historical issues that drove the current design).
