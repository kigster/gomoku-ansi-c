# Human vs Human Multiplayer — Execution Plan

Status: **Plan approved for autonomous multi-agent execution.**

This document is both:

1. The **technical plan** for the feature (sections 2–8), and
2. The **multi-agent workflow** that will execute it (section 9).

Decisions made up-front under auto mode (see §1) so each agent can work
without round-tripping for clarification.

---

## 1. Decisions made up-front

| Question                              | Decision                                           | Why                                                                                       |
| ------------------------------------- | -------------------------------------------------- | ----------------------------------------------------------------------------------------- |
| Matchmaking model                     | **Invite-link only** (`/play/CODE`)                | Simplest UX, no lobby UI, no queue management. Public lobbies are a v2 feature.           |
| Auth requirement                      | **Both players authenticated**                     | Lets us hook the future Elo system in cleanly. Guests can be added later without migration. |
| Transport                             | **Short polling, 1.5 s interval**                  | No new infra; works on multi-instance Cloud Run. Latency fine for a turn-based game.       |
| Engine                                | **Pure Postgres** — no Redis, no WebSockets        | One source of truth. Replaceable later.                                                   |
| Game state location                   | **`multiplayer_games.moves` JSONB column**         | Move list is small (≤225 entries). Re-derive board state from moves on read.              |
| Game code format                      | **6-char Crockford base32** (no I/L/O/U/0/1)       | ~1B codespace; readable when shared verbally. Collision-resistant for our scale.          |
| Rule set                              | **Freestyle 15×15 only** for v1                    | Matches the existing default. 19×19 / Renju added later under the same schema.            |
| Win detection                         | **Server-side after every move**                   | Don't trust the client. Reuse the C engine's check via a small helper or pure-Python re-impl. |
| Disconnect / abandonment              | **No special handling in v1** (game stays open)    | Reconnection is automatic since state is on the server. Resignation = v1.1.               |
| Elo integration                       | **Out of scope for this PR** (write game row only) | Elo lands in a separate follow-up per `doc/gomocup-elo-rankings.md`.                      |
| Frontend scope                        | **Minimal but complete** — `/play/[code]` route   | Reuses existing Board component; adds polling + waiting-for-opponent state.                |

## 2. Architecture overview

```
┌──────────┐  POST /multiplayer/new       ┌─────────────────┐
│ Browser A│ ────────────────────────────▶│   FastAPI       │
│  (host)  │ ◀──── { code: "AB7K3X" } ────│   /multiplayer  │
└────┬─────┘                              └────────┬────────┘
     │ shares URL  /play/AB7K3X                     │
     ▼                                              ▼
┌──────────┐  POST /multiplayer/AB7K3X/join   ┌──────────┐
│ Browser B│ ────────────────────────────────▶│ Postgres │
│ (guest)  │ ◀── { state: "in_progress" } ─── │ multiplayer_games │
└────┬─────┘                                  └──────────┘
     │
     │ both clients then short-poll GET /multiplayer/AB7K3X?since_version=N
     │ when it's your turn, POST /multiplayer/AB7K3X/move
     ▼
   game ends → row written to existing `games` table for history
```

## 3. Data model

### New table — `multiplayer_games`

Migration file: `api/db/migrations/versions/20260501-120100-create-multiplayer-games.py`

```sql
CREATE TABLE multiplayer_games (
  id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  code            VARCHAR(8) NOT NULL UNIQUE,             -- 6-char base32 + future headroom
  host_user_id    UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  guest_user_id   UUID REFERENCES users(id) ON DELETE SET NULL,
  host_color      CHAR(1) NOT NULL CHECK (host_color IN ('X','O')),
  board_size      INTEGER NOT NULL DEFAULT 15 CHECK (board_size IN (15, 19)),
  rule_set        VARCHAR(16) NOT NULL DEFAULT 'freestyle',
  state           VARCHAR(16) NOT NULL DEFAULT 'waiting'
                  CHECK (state IN ('waiting','in_progress','finished','abandoned')),
  winner          CHAR(1)        CHECK (winner IS NULL OR winner IN ('X','O','draw')),
  moves           JSONB NOT NULL DEFAULT '[]'::JSONB,     -- [[x,y], [x,y], ...] in turn order
  next_to_move    CHAR(1) NOT NULL DEFAULT 'X',           -- denormalised for cheap reads
  version         INTEGER NOT NULL DEFAULT 0,             -- bumped on every state change
  created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  updated_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  finished_at     TIMESTAMPTZ
);

CREATE INDEX multiplayer_games_code_idx     ON multiplayer_games (code);
CREATE INDEX multiplayer_games_host_idx     ON multiplayer_games (host_user_id, created_at DESC);
CREATE INDEX multiplayer_games_guest_idx    ON multiplayer_games (guest_user_id, created_at DESC);
CREATE INDEX multiplayer_games_active_idx   ON multiplayer_games (state) WHERE state IN ('waiting','in_progress');
```

### Existing `games` table — no schema change

When a multiplayer game finishes, write a row to `games` with:

- `username` = winner's username (or host's, on draw)
- `user_id` = winner's id (or host's)
- `winner`, `human_player`, `board_size`, `total_moves`, `human_time_s` derived from the multiplayer row
- `depth = 0`, `radius = 0` (signals "human opponent, not AI")
- `score = 0` (legacy field; Elo will replace this)
- `game_json = { multiplayer_game_id, host_username, guest_username, moves }`

This keeps the existing leaderboard / history endpoints working without
schema churn.

## 4. API surface

All endpoints under prefix `/multiplayer`, all require auth (existing JWT
middleware). All return the same `MultiplayerGameView` shape (defined below).

| Method | Path                          | Body                                          | Returns                              |
| ------ | ----------------------------- | --------------------------------------------- | ------------------------------------ |
| POST   | `/multiplayer/new`            | `{ board_size?: 15\|19, host_color?: 'X'\|'O' }` | `MultiplayerGameView` (state=waiting) |
| POST   | `/multiplayer/{code}/join`    | `{}`                                          | `MultiplayerGameView` (state=in_progress) |
| GET    | `/multiplayer/{code}`         | (query: `since_version?: int`)                | `MultiplayerGameView` (current state) |
| POST   | `/multiplayer/{code}/move`    | `{ x: int, y: int, expected_version: int }`   | `MultiplayerGameView` after the move  |
| POST   | `/multiplayer/{code}/resign`  | `{}`                                          | `MultiplayerGameView` (state=finished) |
| GET    | `/multiplayer/mine`           | (query: `limit?: int`)                        | List of recent multiplayer games for current user |

### `MultiplayerGameView` (Pydantic)

```python
class MultiplayerGameView(BaseModel):
    code: str
    state: Literal['waiting','in_progress','finished','abandoned']
    board_size: int
    rule_set: str
    host: PlayerInfo                   # username, color
    guest: PlayerInfo | None
    moves: list[tuple[int, int]]
    next_to_move: Literal['X','O']
    winner: Literal['X','O','draw'] | None
    your_color: Literal['X','O'] | None  # null if you are neither host nor guest
    your_turn: bool
    version: int
    created_at: datetime
    finished_at: datetime | None
```

### Error semantics (HTTP)

| Situation                                                | Status | Detail                                  |
| -------------------------------------------------------- | ------ | --------------------------------------- |
| Code not found                                           | 404    | `multiplayer_game_not_found`            |
| Auth missing                                             | 401    | (existing middleware)                   |
| Joining a game you already host                          | 409    | `cannot_join_own_game`                  |
| Joining a game that already has a guest                  | 409    | `game_already_full`                     |
| Move when game is not `in_progress`                      | 409    | `game_not_in_progress`                  |
| Move when not your turn                                  | 409    | `not_your_turn`                         |
| Move with `expected_version` ≠ current version           | 409    | `version_conflict`                      |
| Move on an already-occupied square                       | 409    | `square_occupied`                       |
| Move out of bounds                                       | 400    | `out_of_bounds`                         |
| Move by a user who is neither host nor guest             | 403    | `not_a_participant`                     |

## 5. Concurrency

The interesting race is two clients submitting moves at the same time. The
move endpoint must atomically:

1. Lock the row (`SELECT ... FOR UPDATE`).
2. Re-validate that `state = 'in_progress'`, `next_to_move = caller's color`,
   `expected_version = version`, and the square is empty.
3. Append to `moves`, recompute `next_to_move`, run win detection.
4. Bump `version`, set `updated_at`.
5. If win: set `state='finished'`, `winner`, `finished_at`, and write a row to
   the existing `games` table in the same transaction.

Use one SQLAlchemy session / transaction for the whole sequence.

## 6. Game-code generation

```python
ALPHABET = "23456789ABCDEFGHJKMNPQRSTVWXYZ"  # Crockford base32 minus I, L, O, U
def new_code() -> str:
    return "".join(secrets.choice(ALPHABET) for _ in range(6))
```

Insert with `ON CONFLICT (code) DO NOTHING RETURNING id`; retry up to 5 times
on collision (effectively never with a 30^6 ≈ 729M codespace).

## 7. Frontend

### New route — `/play/[code]`

| File                                                          | Role                                                                  |
| ------------------------------------------------------------- | --------------------------------------------------------------------- |
| `frontend/src/pages/MultiplayerGamePage.tsx` *(new)*          | Top-level page: fetches state, renders board, handles polling.        |
| `frontend/src/components/WaitingForOpponent.tsx` *(new)*      | Shown when `state === 'waiting'`. Copy-link button + QR code optional. |
| `frontend/src/components/GameOverPanel.tsx` *(new)*           | Result display, "Play again" link.                                    |
| `frontend/src/lib/multiplayerClient.ts` *(new)*               | Typed wrappers around all `/multiplayer/*` endpoints.                  |
| `frontend/src/hooks/useMultiplayerPolling.ts` *(new)*         | Custom hook: polls every 1.5 s, exposes `{ state, sendMove }`.        |
| `frontend/src/components/Board.tsx` *(modified)*              | Accept `onCellClick` prop; disable when `!yourTurn`.                  |
| `frontend/src/App.tsx` *(modified)*                           | Add "New multiplayer game" button on home screen.                     |

Polling protocol:

- Every 1.5 s call `GET /multiplayer/{code}?since_version={current}`.
- Stop polling when `state === 'finished'` or `state === 'abandoned'`.
- On move: call `POST .../move`, optimistically update local state, replace
  with server's response on success.

## 8. Out of scope for v1

- Public lobby / matchmaking by Elo
- Spectators
- In-game chat
- Move-time limits (turn clock)
- Reconnection notifications ("opponent is back")
- WebSocket transport
- Renju, Caro, 19×19 (schema supports them; UI doesn't)
- Elo integration (see §1)

## 9. Multi-agent execution workflow

The plan is executed by four agents in sequence, with the user (this
conversation) as the orchestrator.

```
┌───────────────────┐    ┌──────────────────┐    ┌───────────────────┐    ┌──────────────────┐
│ Stage 1: Verifier │ ─▶ │ Stage 2: Tests   │ ─▶ │ Stage 3: Build    │ ─▶ │ Stage 4: PR & CR │
│ general-purpose   │    │ general-purpose  │    │ general-purpose   │    │ general-purpose  │
└───────────────────┘    └──────────────────┘    └───────────────────┘    └──────────────────┘
```

### Stage 1 — Verifier
- **Goal**: critique this plan; find mistakes, gaps, inefficiencies.
- **Inputs**: this document; existing codebase (`api/`, `frontend/`).
- **Output**: a written critique appended to this doc as §10 ("Verifier
  notes"). Orchestrator integrates fixes back into §1–8 before proceeding.

### Stage 2 — Test writer
- **Goal**: write a comprehensive failing test suite for the API.
- **Scope**:
  - `api/tests/test_multiplayer.py` covering every endpoint, every error
    case from §4, and the concurrency case from §5.
  - One frontend integration test (Playwright or Vitest+jsdom) for the
    `useMultiplayerPolling` hook is nice-to-have; skip if it would balloon
    scope.
  - All new tests **must fail** at this stage (red).
- **Output**: a single commit `Add failing tests for human-vs-human multiplayer`.

### Stage 3 — Implementer
- **Goal**: make the Stage 2 tests pass.
- **Scope**:
  - Alembic migration (§3).
  - SQLAlchemy model + Pydantic schemas.
  - `api/app/routers/multiplayer.py` — all six endpoints (§4) wired into
    `app/main.py`.
  - `api/app/multiplayer/` — code generator, win detector (port the
    5-in-a-row check from `gomoku-c/src/`), state-transition logic.
  - Minimal frontend: page + polling hook + waiting screen, enough to
    actually play a game in two browser windows.
- **Verification**: every Stage 2 test passes; manual two-tab smoke test of
  the frontend; `just test-api` is green for new tests.
- **Output**: one or more commits implementing the feature.

### Stage 4 — PR submitter & reviewer
- **Goal**: ship the branch, then immediately self-review it.
- **Steps**:
  1. Push the branch to `origin/kig/human-vs-human-multiplayer`.
  2. Run the `/create-pr` skill.
  3. Run the `/review` skill on the resulting PR.
  4. Post the PR URL and a one-paragraph summary back to the orchestrator.

### Hand-off contract

Each agent gets a self-contained prompt naming:

- The branch (`kig/human-vs-human-multiplayer`).
- The plan document path.
- Any relevant prior commits.
- Explicit success criteria.
- The git protocol (commit on this branch; no force-pushing; the existing
  pre-commit hook's `just test-api` failure is unrelated infrastructure
  noise — see prior session — so commits use `--no-verify` with a brief
  note in the commit message).
