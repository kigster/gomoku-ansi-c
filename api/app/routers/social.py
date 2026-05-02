"""Social-graph router: /social/follow, /social/unfollow, /social/block.

Implements the unidirectional follow + block model surfaced by the chat
panel's slash commands. See frontend/src/components/ChatPanel.tsx for
the contract this router has to satisfy:

- POST /social/follow { target_username }
    → 200 { reciprocal: bool }   # true iff target also follows caller
    → 404 user_not_found

- POST /social/unfollow { target_username }
    → 200 { unfollowed: true }   # idempotent — true even when nothing
                                  # was followed in the first place
    → 404 user_not_found

- POST /social/block { target_username }
    → 200 { game_terminated: bool }   # true iff there was an active
                                       # multiplayer game between the
                                       # two — block always terminates.
    → 404 user_not_found

Game termination on social actions is **block-only**. An unfollow
deliberately does NOT cascade into game termination, even when it
severs the last social link between two players: a game's liveness
shouldn't depend on social-graph state that neither participant can
see in the game UI. Only an explicit /block (or in-game resign /
timeout) ends a game.
"""

from __future__ import annotations

import asyncpg
from fastapi import APIRouter, Depends, HTTPException, status
from pydantic import BaseModel, Field

from app.database import get_pool
from app.security import get_current_user

router = APIRouter(prefix="/social", tags=["social"])


class TargetUsernameRequest(BaseModel):
    """Body shape shared by all three endpoints."""

    target_username: str = Field(min_length=2, max_length=30)


class FollowResponse(BaseModel):
    reciprocal: bool


class UnfollowResponse(BaseModel):
    """Idempotent — `unfollowed: true` whether or not a row was deleted.

    Deliberately does NOT include a `game_terminated` field. Unfollow
    is a pure social-graph operation; only /block (or in-game
    resign / timeout) ends games.
    """

    unfollowed: bool


class BlockResponse(BaseModel):
    game_terminated: bool


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


async def _resolve_target(conn: asyncpg.Connection, username: str, *, caller_id: str) -> dict:
    """Look up the target user, 404 if missing or self-targeted."""
    row = await conn.fetchrow(
        "SELECT id, username FROM users WHERE lower(username) = lower($1)",
        username,
    )
    if row is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "user_not_found")
    if str(row["id"]) == caller_id:
        # The CHECK on friendships / blocks would catch self-targeting too,
        # but a clear 400 beats a 500 from a constraint violation.
        raise HTTPException(status.HTTP_400_BAD_REQUEST, "cannot_target_self")
    return dict(row)


async def _terminate_active_game_between(
    conn: asyncpg.Connection, user_a: str, user_b: str
) -> bool:
    """If there's a `waiting` or `in_progress` multiplayer game between A
    and B, mark it terminated and return True. Otherwise return False.

    Waiting → cancelled (the game never started, no result to record).
    In-progress → abandoned (preserves the partial moves but ends the game).
    """
    row = await conn.fetchrow(
        """
        UPDATE multiplayer_games
        SET    state      = CASE state
                              WHEN 'waiting' THEN 'cancelled'
                              ELSE 'abandoned'
                            END,
               version    = version + 1,
               updated_at = NOW(),
               finished_at = NOW()
        WHERE  state IN ('waiting', 'in_progress')
          AND  (
                  (host_user_id = $1::uuid AND guest_user_id = $2::uuid)
               OR (host_user_id = $2::uuid AND guest_user_id = $1::uuid)
              )
        RETURNING id
        """,
        user_a,
        user_b,
    )
    return row is not None


# ---------------------------------------------------------------------------
# Endpoints
# ---------------------------------------------------------------------------


@router.post("/follow", response_model=FollowResponse)
async def follow(
    body: TargetUsernameRequest,
    user: dict = Depends(get_current_user),
    pool=Depends(get_pool),
) -> FollowResponse:
    caller_id = str(user["id"])
    async with pool.acquire() as conn:
        target = await _resolve_target(conn, body.target_username, caller_id=caller_id)
        target_id = str(target["id"])
        # Idempotent insert — re-following an existing follow is a no-op.
        await conn.execute(
            """
            INSERT INTO friendships (user_id, friend_id)
            VALUES ($1::uuid, $2::uuid)
            ON CONFLICT (user_id, friend_id) DO NOTHING
            """,
            caller_id,
            target_id,
        )
        # Reciprocity check: is there a row in the OTHER direction?
        reciprocal_row = await conn.fetchrow(
            """
            SELECT 1 FROM friendships
            WHERE user_id = $1::uuid AND friend_id = $2::uuid
            """,
            target_id,
            caller_id,
        )
    return FollowResponse(reciprocal=reciprocal_row is not None)


@router.post("/unfollow", response_model=UnfollowResponse)
async def unfollow(
    body: TargetUsernameRequest,
    user: dict = Depends(get_current_user),
    pool=Depends(get_pool),
) -> UnfollowResponse:
    caller_id = str(user["id"])
    async with pool.acquire() as conn:
        target = await _resolve_target(conn, body.target_username, caller_id=caller_id)
        target_id = str(target["id"])
        # Idempotent — DELETE returns affected rows but we don't care;
        # the result is the same whether anything was actually followed.
        await conn.execute(
            """
            DELETE FROM friendships
            WHERE user_id = $1::uuid AND friend_id = $2::uuid
            """,
            caller_id,
            target_id,
        )
        # Game termination is intentionally NOT triggered here —
        # see the module docstring. Only /social/block (or explicit
        # in-game resign / timeout) ends a game.
    return UnfollowResponse(unfollowed=True)


@router.post("/block", response_model=BlockResponse)
async def block(
    body: TargetUsernameRequest,
    user: dict = Depends(get_current_user),
    pool=Depends(get_pool),
) -> BlockResponse:
    caller_id = str(user["id"])
    async with pool.acquire() as conn:
        target = await _resolve_target(conn, body.target_username, caller_id=caller_id)
        target_id = str(target["id"])
        async with conn.transaction():
            # Idempotent insert.
            await conn.execute(
                """
                INSERT INTO blocks (blocker_id, blocked_id)
                VALUES ($1::uuid, $2::uuid)
                ON CONFLICT (blocker_id, blocked_id) DO NOTHING
                """,
                caller_id,
                target_id,
            )
            # A block also wipes any follow in either direction so the
            # blocked user can't piggy-back on a stale follow to keep
            # invite-spamming.
            await conn.execute(
                """
                DELETE FROM friendships
                WHERE (user_id = $1::uuid AND friend_id = $2::uuid)
                   OR (user_id = $2::uuid AND friend_id = $1::uuid)
                """,
                caller_id,
                target_id,
            )
            # Always terminate any active game between the two.
            game_terminated = await _terminate_active_game_between(
                conn, caller_id, target_id
            )
    return BlockResponse(game_terminated=game_terminated)
