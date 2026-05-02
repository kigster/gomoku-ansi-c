"""Shared game-code allocation with collision retry.

Both `app.routers.multiplayer::create_game` (POST /multiplayer/new) and
`app.routers.chat::invite` (POST /chat/invite) need to insert a new
multiplayer_games row keyed by a randomly-generated 6-char Crockford
code. The codespace is ~729M (32**6 minus the ambiguous I/L/O/U/0/1
letters Crockford excludes), so collisions are astronomically rare —
but they do exist, and a single user-facing 5xx because of one is a
poor experience. We retry up to MAX_RETRIES times, each attempt
wrapped in a savepoint so a UniqueViolationError doesn't poison the
caller's surrounding transaction.

This helper is the single source of truth for that logic — both
routers call it. Adding new code-allocation call sites in the future
should also use it.

`created_via` ('modal' | 'invite') is required so the chat-invite
rate limit can count only invite rows. Required (no default) so
type-checkers flag any new call site that forgets it.
"""

from __future__ import annotations

from typing import Literal

import asyncpg

from app.multiplayer.codes import new_code

# Eight retries gives us a (1/729e6)**8 chance of an unrecoverable
# collision — call it never. Anything more would just delay the eventual
# 5xx without changing the user-visible outcome.
MAX_RETRIES = 8

CreatedVia = Literal["modal", "invite"]


async def allocate_game(
    conn: asyncpg.Connection,
    *,
    host_user_id: str,
    created_via: CreatedVia,
    host_color: Literal["X", "O"] | None = "X",
    board_size: int = 15,
    color_chosen_by: str = "host",
) -> dict:
    """Insert a new multiplayer_games row with a unique code.

    Returns the inserted row as a dict (callers that only need the
    code take `row["code"]`). Retries up to MAX_RETRIES times on
    UniqueViolationError. Each attempt uses a savepoint so a
    collision doesn't poison the caller's transaction.

    Raises RuntimeError if all attempts fail (astronomically unlikely
    with the ~729M codespace).
    """
    last_exc: Exception | None = None
    for _ in range(MAX_RETRIES):
        candidate = new_code()
        try:
            async with conn.transaction():
                row = await conn.fetchrow(
                    """
                    INSERT INTO multiplayer_games
                        (code, host_user_id, host_color, board_size,
                         color_chosen_by, created_via)
                    VALUES ($1, $2::uuid, $3, $4, $5, $6)
                    RETURNING *
                    """,
                    candidate,
                    host_user_id,
                    host_color,
                    board_size,
                    color_chosen_by,
                    created_via,
                )
            if row is not None:
                return dict(row)
        except asyncpg.UniqueViolationError as exc:
            last_exc = exc
            continue
    raise RuntimeError(
        f"Failed to allocate game code after {MAX_RETRIES} attempts: {last_exc}"
    )
