"""Human-vs-human multiplayer router.

See:

- `doc/human-vs-human-plan.md` §4 for the original API surface and §5 for
  the concurrency rules.
- `doc/multiplayer-modal-plan.md` for the host-chooses / guest-chooses
  color flow, the 15-minute invite expiry, and the cancel endpoint.
- `doc/multiplayer-bugs.md` for the issues that this version addresses
  (collision retry on code generation, board-size move validation,
  game_type discriminator on the `games` history table, surfacing of
  cancellation/expiry to the client, etc.).

All endpoints require authentication (`get_current_user`). Codes are
6-char Crockford base32 (`app.multiplayer.codes`). Win detection is
`count == 5` (matches the C engine — see
`gomoku-c/src/gomoku/gomoku.c:180`).
"""

from __future__ import annotations

import json as json_mod
from typing import Any

import asyncpg
from fastapi import APIRouter, Depends, HTTPException, Query, Response, status
from fastapi.responses import JSONResponse

from app.config import settings
from app.database import get_pool
from app.models.multiplayer import (
    CancelRequest,
    JoinRequest,
    MoveRequest,
    MultiplayerGamePreview,
    MultiplayerGameView,
    NewMultiplayerGameRequest,
    PlayerInfo,
    ResignRequest,
)
from app.multiplayer.codes import new_code
from app.multiplayer.win_detector import has_winner
from app.security import get_current_user

router = APIRouter(prefix="/multiplayer", tags=["multiplayer"])

_MAX_CODE_RETRIES = 8


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _coerce_moves(raw: Any) -> list[tuple[int, int]]:
    """Convert the JSONB `moves` column to list[tuple[int,int]].

    asyncpg may hand us either a Python list (if a JSONB type codec is in use)
    or a JSON string. Normalise both.
    """
    if raw is None:
        return []
    if isinstance(raw, str):
        raw = json_mod.loads(raw)
    return [(int(m[0]), int(m[1])) for m in raw]


def _opposite_color(color: str) -> str:
    return "O" if color == "X" else "X"


def _participant_color(row: dict, user_id: str) -> str | None:
    """Return 'X' / 'O' if `user_id` is host/guest, else None.

    When `host_color is None` (host let the guest pick and they haven't
    joined yet), we still need to identify the host as a participant —
    fall back to the role rather than the colour."""
    if str(row["host_user_id"]) == user_id:
        return row["host_color"]  # may be None for unresolved guest-chooses
    if row["guest_user_id"] is not None and str(row["guest_user_id"]) == user_id:
        host_color = row["host_color"]
        return _opposite_color(host_color) if host_color else None
    return None


def _is_participant(row: dict, user_id: str) -> bool:
    return str(row["host_user_id"]) == user_id or (
        row["guest_user_id"] is not None and str(row["guest_user_id"]) == user_id
    )


def _invite_url(code: str) -> str:
    """Public URL the host shares with the guest."""
    domain = settings.public_domain
    scheme = "http" if domain.startswith("localhost") or domain.startswith("127.") else "https"
    return f"{scheme}://{domain}/play/{code}"


def _build_view(
    row: dict,
    *,
    host_username: str,
    guest_username: str | None,
    your_color: str | None,
) -> MultiplayerGameView:
    """Assemble the full participant view from a DB row."""
    moves = _coerce_moves(row["moves"])
    host_color = row["host_color"]
    next_to_move = row["next_to_move"]
    your_turn = (
        your_color is not None
        and row["state"] == "in_progress"
        and your_color == next_to_move
    )
    guest_color = (
        _opposite_color(host_color) if (host_color and guest_username) else None
    )
    return MultiplayerGameView(
        code=row["code"],
        state=row["state"],
        board_size=row["board_size"],
        rule_set=row["rule_set"],
        host=PlayerInfo(username=host_username, color=host_color),
        guest=(
            PlayerInfo(username=guest_username, color=guest_color)
            if guest_username
            else None
        ),
        moves=moves,
        next_to_move=next_to_move,
        winner=row["winner"],
        your_color=your_color,
        your_turn=your_turn,
        version=row["version"],
        color_chosen_by=row["color_chosen_by"],
        expires_at=row["expires_at"],
        created_at=row["created_at"],
        finished_at=row["finished_at"],
        invite_url=_invite_url(row["code"]),
    )


def _build_preview(
    row: dict,
    *,
    host_username: str,
    guest_username: str | None,
) -> MultiplayerGamePreview:
    host_color = row["host_color"]
    guest_color = (
        _opposite_color(host_color) if (host_color and guest_username) else None
    )
    return MultiplayerGamePreview(
        code=row["code"],
        state=row["state"],
        board_size=row["board_size"],
        rule_set=row["rule_set"],
        host=PlayerInfo(username=host_username, color=host_color),
        guest=(
            PlayerInfo(username=guest_username, color=guest_color)
            if guest_username
            else None
        ),
        next_to_move=row["next_to_move"],
        winner=row["winner"],
        version=row["version"],
        color_chosen_by=row["color_chosen_by"],
        expires_at=row["expires_at"],
        created_at=row["created_at"],
        finished_at=row["finished_at"],
    )


async def _expire_if_stale(conn, code: str) -> None:
    """Lazy expiry: if a `waiting` game is past its TTL, mark it `cancelled`.

    The expiry-bumps-version semantics mean polling clients will see the
    state change on their next poll. Per `doc/multiplayer-modal-plan.md`
    §3 — no background sweeper required for the modal flow.
    """
    await conn.execute(
        """
        UPDATE multiplayer_games
        SET    state      = 'cancelled',
               version    = version + 1,
               updated_at = NOW()
        WHERE  code = $1
          AND  state = 'waiting'
          AND  expires_at <= NOW()
        """,
        code,
    )


async def _fetch_with_usernames(conn, code: str) -> dict | None:
    """Return a dict with all multiplayer_games columns plus
    `host_username` and `guest_username`, or None if missing."""
    row = await conn.fetchrow(
        """
        SELECT mg.*,
               hu.username AS host_username,
               gu.username AS guest_username
        FROM multiplayer_games mg
        JOIN users hu ON hu.id = mg.host_user_id
        LEFT JOIN users gu ON gu.id = mg.guest_user_id
        WHERE mg.code = $1
        """,
        code,
    )
    return dict(row) if row else None


async def _insert_game(
    conn,
    *,
    host_user_id: str,
    host_color: str | None,
    color_chosen_by: str,
    board_size: int,
) -> dict:
    """Insert a new multiplayer game with a unique code; returns the row dict.

    Retries up to `_MAX_CODE_RETRIES` on `UniqueViolationError` to handle
    the (vanishingly unlikely) 6-char-Crockford collision — see
    `doc/multiplayer-bugs.md` item #4.
    """
    last_exc: Exception | None = None
    for _ in range(_MAX_CODE_RETRIES):
        code = new_code()
        try:
            # Per-attempt savepoint — asyncpg's nested `conn.transaction()`
            # opens a SAVEPOINT, so a UniqueViolation rolls back only this
            # attempt and leaves the parent transaction usable for the
            # next try (or the caller's downstream work).
            async with conn.transaction():
                row = await conn.fetchrow(
                    """
                    INSERT INTO multiplayer_games (
                        code, host_user_id, host_color, color_chosen_by, board_size
                    )
                    VALUES ($1, $2::uuid, $3, $4, $5)
                    RETURNING *
                    """,
                    code,
                    host_user_id,
                    host_color,
                    color_chosen_by,
                    board_size,
                )
            if row is not None:
                return dict(row)
        except asyncpg.UniqueViolationError as exc:
            last_exc = exc
            continue
    raise HTTPException(
        status.HTTP_503_SERVICE_UNAVAILABLE,
        f"Failed to allocate a unique multiplayer code: {last_exc}",
    )


async def _write_finished_games_rows(
    conn,
    *,
    mp_row: dict,
    host_username: str,
    guest_username: str,
    winner: str,
    moves: list[tuple[int, int]],
) -> None:
    """Write two `games` rows, one per participant, when a multiplayer game ends.

    `game_type='multiplayer'` admits the depth/radius/total_moves zero
    sentinels (see migration 0006 + `doc/multiplayer-bugs.md` item #1).
    """
    assert mp_row["guest_user_id"] is not None, (
        "_write_finished_games_rows must only be called once a guest has joined"
    )
    game_json = json_mod.dumps(
        {
            "multiplayer_game_id": str(mp_row["id"]),
            "host_username": host_username,
            "guest_username": guest_username,
            "moves": [list(m) for m in moves],
            "rule_set": mp_row["rule_set"],
            "board_size": mp_row["board_size"],
            "winner": winner,
        }
    )
    total_moves = len(moves)
    host_color = mp_row["host_color"]
    guest_color = _opposite_color(host_color)
    host_user_id = str(mp_row["host_user_id"])
    guest_user_id = str(mp_row["guest_user_id"])

    insert_sql = """
        INSERT INTO games
          (username, user_id, winner, human_player, board_size, depth, radius,
           total_moves, human_time_s, ai_time_s, score, game_json, game_type)
        VALUES ($1, $2::uuid, $3, $4, $5, 0, 0,
                $6, 0, 0, 0, $7::jsonb, 'multiplayer')
    """
    # Host row
    await conn.execute(
        insert_sql,
        host_username,
        host_user_id,
        winner,
        host_color,
        mp_row["board_size"],
        total_moves,
        game_json,
    )
    # Guest row
    await conn.execute(
        insert_sql,
        guest_username,
        guest_user_id,
        winner,
        guest_color,
        mp_row["board_size"],
        total_moves,
        game_json,
    )


# ---------------------------------------------------------------------------
# Endpoints
# ---------------------------------------------------------------------------


@router.post("/new", response_model=MultiplayerGameView)
async def new_game(
    body: NewMultiplayerGameRequest,
    user: dict = Depends(get_current_user),
    pool=Depends(get_pool),
):
    color_chosen_by = "host" if body.host_color is not None else "guest"
    async with pool.acquire() as conn:
        async with conn.transaction():
            row = await _insert_game(
                conn,
                host_user_id=str(user["id"]),
                host_color=body.host_color,
                color_chosen_by=color_chosen_by,
                board_size=body.board_size,
            )
    your_color = body.host_color  # may be None when guest chooses
    return _build_view(
        row,
        host_username=user["username"],
        guest_username=None,
        your_color=your_color,
    )


@router.get("/mine")
async def my_games(
    limit: int = Query(default=50, ge=1, le=200),
    user: dict = Depends(get_current_user),
    pool=Depends(get_pool),
):
    """Return the caller's recent multiplayer games (host or guest), DESC by created_at."""
    user_id = str(user["id"])
    rows = await pool.fetch(
        """
        SELECT mg.*,
               hu.username AS host_username,
               gu.username AS guest_username
        FROM multiplayer_games mg
        JOIN users hu ON hu.id = mg.host_user_id
        LEFT JOIN users gu ON gu.id = mg.guest_user_id
        WHERE mg.host_user_id = $1::uuid OR mg.guest_user_id = $1::uuid
        ORDER BY mg.created_at DESC
        LIMIT $2
        """,
        user_id,
        limit,
    )
    out: list[dict] = []
    for r in rows:
        rd = dict(r)
        your_color = _participant_color(rd, user_id)
        view = _build_view(
            rd,
            host_username=rd["host_username"],
            guest_username=rd["guest_username"],
            your_color=your_color,
        )
        out.append(view.model_dump(mode="json"))
    return out


@router.post("/{code}/join", response_model=MultiplayerGameView)
async def join_game(
    code: str,
    body: JoinRequest,
    user: dict = Depends(get_current_user),
    pool=Depends(get_pool),
):
    user_id = str(user["id"])
    async with pool.acquire() as conn:
        async with conn.transaction():
            await _expire_if_stale(conn, code)

            existing_rec = await conn.fetchrow(
                "SELECT * FROM multiplayer_games WHERE code = $1 FOR UPDATE",
                code,
            )
            if existing_rec is None:
                raise HTTPException(
                    status.HTTP_404_NOT_FOUND, "multiplayer_game_not_found"
                )
            existing = dict(existing_rec)

            # Pre-flight checks before mutating.
            if str(existing["host_user_id"]) == user_id:
                raise HTTPException(
                    status.HTTP_409_CONFLICT, "cannot_join_own_game"
                )
            if existing["state"] == "cancelled":
                raise HTTPException(
                    status.HTTP_409_CONFLICT, "game_cancelled"
                )
            if existing["guest_user_id"] is not None:
                raise HTTPException(
                    status.HTTP_409_CONFLICT, "game_already_full"
                )
            if existing["state"] != "waiting":
                raise HTTPException(
                    status.HTTP_409_CONFLICT, "game_not_in_waiting_state"
                )

            color_chosen_by = existing["color_chosen_by"]
            chosen = body.chosen_color
            if color_chosen_by == "guest":
                if chosen is None:
                    raise HTTPException(
                        status.HTTP_422_UNPROCESSABLE_ENTITY,
                        "chosen_color_required",
                    )
                # Guest picks their colour; host gets the opposite.
                new_host_color = _opposite_color(chosen)
            else:
                if chosen is not None:
                    raise HTTPException(
                        status.HTTP_422_UNPROCESSABLE_ENTITY,
                        "chosen_color_not_allowed",
                    )
                new_host_color = existing["host_color"]

            updated = await conn.fetchrow(
                """
                UPDATE multiplayer_games
                SET    guest_user_id = $1::uuid,
                       host_color    = $3,
                       state         = 'in_progress',
                       version       = version + 1,
                       updated_at    = NOW()
                WHERE  code = $2
                RETURNING *
                """,
                user_id,
                code,
                new_host_color,
            )
            row = dict(updated)
            host_username = await conn.fetchval(
                "SELECT username FROM users WHERE id = $1::uuid",
                str(row["host_user_id"]),
            )

    your_color = _opposite_color(row["host_color"])
    return _build_view(
        row,
        host_username=host_username,
        guest_username=user["username"],
        your_color=your_color,
    )


@router.post("/{code}/cancel", response_model=MultiplayerGameView)
async def cancel_game(
    code: str,
    body: CancelRequest,  # noqa: ARG001
    user: dict = Depends(get_current_user),
    pool=Depends(get_pool),
):
    """Host-only: cancel a `waiting` game. Marks it `cancelled` in the DB."""
    user_id = str(user["id"])
    async with pool.acquire() as conn:
        async with conn.transaction():
            row_rec = await conn.fetchrow(
                """
                SELECT mg.*,
                       hu.username AS host_username,
                       gu.username AS guest_username
                FROM multiplayer_games mg
                JOIN users hu ON hu.id = mg.host_user_id
                LEFT JOIN users gu ON gu.id = mg.guest_user_id
                WHERE mg.code = $1
                FOR UPDATE OF mg
                """,
                code,
            )
            if row_rec is None:
                raise HTTPException(
                    status.HTTP_404_NOT_FOUND, "multiplayer_game_not_found"
                )
            row = dict(row_rec)

            if str(row["host_user_id"]) != user_id:
                raise HTTPException(
                    status.HTTP_403_FORBIDDEN, "not_the_host"
                )
            if row["state"] != "waiting":
                raise HTTPException(
                    status.HTTP_409_CONFLICT,
                    f"cannot_cancel_in_state_{row['state']}",
                )

            updated = await conn.fetchrow(
                """
                UPDATE multiplayer_games
                SET    state      = 'cancelled',
                       version    = version + 1,
                       updated_at = NOW()
                WHERE  id = $1
                RETURNING *
                """,
                row["id"],
            )
            row = dict(updated)

    your_color = _participant_color(row, user_id)
    return _build_view(
        row,
        host_username=row_rec["host_username"],
        guest_username=row_rec["guest_username"],
        your_color=your_color,
    )


@router.get("/{code}")
async def get_game(
    code: str,
    response: Response,  # noqa: ARG001
    since_version: int | None = Query(default=None, ge=0),
    user: dict = Depends(get_current_user),
    pool=Depends(get_pool),
):
    async with pool.acquire() as conn:
        await _expire_if_stale(conn, code)
        row = await _fetch_with_usernames(conn, code)
    if row is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "multiplayer_game_not_found")

    if since_version is not None and row["version"] <= since_version:
        return Response(status_code=status.HTTP_304_NOT_MODIFIED)

    user_id = str(user["id"])
    is_host = str(row["host_user_id"]) == user_id
    is_guest = (
        row["guest_user_id"] is not None and str(row["guest_user_id"]) == user_id
    )

    if not (is_host or is_guest):
        preview = _build_preview(
            row,
            host_username=row["host_username"],
            guest_username=row["guest_username"],
        )
        return JSONResponse(content=preview.model_dump(mode="json"))

    your_color = _participant_color(row, user_id)
    view = _build_view(
        row,
        host_username=row["host_username"],
        guest_username=row["guest_username"],
        your_color=your_color,
    )
    return JSONResponse(content=view.model_dump(mode="json"))


@router.post("/{code}/move", response_model=MultiplayerGameView)
async def make_move(
    code: str,
    body: MoveRequest,
    user: dict = Depends(get_current_user),
    pool=Depends(get_pool),
):
    user_id = str(user["id"])
    async with pool.acquire() as conn:
        async with conn.transaction():
            row_rec = await conn.fetchrow(
                """
                SELECT mg.*, hu.username AS host_username, gu.username AS guest_username
                FROM multiplayer_games mg
                JOIN users hu ON hu.id = mg.host_user_id
                LEFT JOIN users gu ON gu.id = mg.guest_user_id
                WHERE mg.code = $1
                FOR UPDATE OF mg
                """,
                code,
            )
            if row_rec is None:
                raise HTTPException(
                    status.HTTP_404_NOT_FOUND, "multiplayer_game_not_found"
                )
            row = dict(row_rec)

            your_color = _participant_color(row, user_id)
            if your_color is None:
                raise HTTPException(
                    status.HTTP_403_FORBIDDEN, "not_a_participant"
                )

            if row["state"] != "in_progress":
                raise HTTPException(
                    status.HTTP_409_CONFLICT, "game_not_in_progress"
                )

            x, y = int(body.x), int(body.y)
            board_size = row["board_size"]
            # Single canonical OOB check — keeps the wire contract on a
            # consistent 400 regardless of which axis or whether the value
            # would also fail a Pydantic upper bound (see
            # doc/multiplayer-bugs.md item #7).
            if not (0 <= x < board_size and 0 <= y < board_size):
                raise HTTPException(
                    status.HTTP_400_BAD_REQUEST, "out_of_bounds"
                )

            if body.expected_version != row["version"]:
                raise HTTPException(
                    status.HTTP_409_CONFLICT, "version_conflict"
                )

            if your_color != row["next_to_move"]:
                raise HTTPException(
                    status.HTTP_409_CONFLICT, "not_your_turn"
                )

            moves = _coerce_moves(row["moves"])
            if (x, y) in {(mx, my) for mx, my in moves}:
                raise HTTPException(
                    status.HTTP_409_CONFLICT, "square_occupied"
                )

            moves.append((x, y))

            won = has_winner(moves, x, y, your_color, board_size)
            new_state = "finished" if won else "in_progress"
            new_winner = your_color if won else None
            next_to_move = (
                row["next_to_move"]
                if won
                else _opposite_color(row["next_to_move"])
            )

            new_moves_json = json_mod.dumps([list(m) for m in moves])

            updated = await conn.fetchrow(
                """
                UPDATE multiplayer_games
                SET    moves         = $1::jsonb,
                       next_to_move  = $2,
                       version       = version + 1,
                       updated_at    = NOW(),
                       state         = $3::varchar,
                       winner        = $4,
                       finished_at   = CASE
                           WHEN $3::varchar = 'finished' THEN NOW()
                           ELSE finished_at
                       END
                WHERE  id = $5
                RETURNING *
                """,
                new_moves_json,
                next_to_move,
                new_state,
                new_winner,
                row["id"],
            )
            updated_row = dict(updated)

            if won:
                await _write_finished_games_rows(
                    conn,
                    mp_row=updated_row,
                    host_username=row["host_username"],
                    guest_username=row["guest_username"],
                    winner=new_winner,
                    moves=moves,
                )

    return _build_view(
        updated_row,
        host_username=row["host_username"],
        guest_username=row["guest_username"],
        your_color=your_color,
    )


@router.post("/{code}/resign", response_model=MultiplayerGameView)
async def resign_game(
    code: str,
    body: ResignRequest,  # noqa: ARG001
    user: dict = Depends(get_current_user),
    pool=Depends(get_pool),
):
    user_id = str(user["id"])
    async with pool.acquire() as conn:
        async with conn.transaction():
            row_rec = await conn.fetchrow(
                """
                SELECT mg.*, hu.username AS host_username, gu.username AS guest_username
                FROM multiplayer_games mg
                JOIN users hu ON hu.id = mg.host_user_id
                LEFT JOIN users gu ON gu.id = mg.guest_user_id
                WHERE mg.code = $1
                FOR UPDATE OF mg
                """,
                code,
            )
            if row_rec is None:
                raise HTTPException(
                    status.HTTP_404_NOT_FOUND, "multiplayer_game_not_found"
                )
            row = dict(row_rec)

            your_color = _participant_color(row, user_id)
            if your_color is None:
                raise HTTPException(
                    status.HTTP_403_FORBIDDEN, "not_a_participant"
                )
            if row["state"] != "in_progress":
                raise HTTPException(
                    status.HTTP_409_CONFLICT, "game_not_in_progress"
                )

            winner = _opposite_color(your_color)
            updated = await conn.fetchrow(
                """
                UPDATE multiplayer_games
                SET    state       = 'finished',
                       winner      = $1,
                       version     = version + 1,
                       updated_at  = NOW(),
                       finished_at = NOW()
                WHERE  id = $2
                RETURNING *
                """,
                winner,
                row["id"],
            )
            updated_row = dict(updated)

            moves = _coerce_moves(updated_row["moves"])
            await _write_finished_games_rows(
                conn,
                mp_row=updated_row,
                host_username=row["host_username"],
                guest_username=row["guest_username"],
                winner=winner,
                moves=moves,
            )

    return _build_view(
        updated_row,
        host_username=row["host_username"],
        guest_username=row["guest_username"],
        your_color=your_color,
    )
