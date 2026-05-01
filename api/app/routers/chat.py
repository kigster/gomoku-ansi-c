"""Chat router.

For now this router exposes a single endpoint, /chat/invite, which the
ChatPanel slash-command parser hits when the user types
`/invite @username`. The full chat-message persistence + polling layer
comes in a follow-up PR; we land /invite first because it's the only
slash command with cross-user side effects (the others are pure
social-graph writes handled in app/routers/social.py).

POST /chat/invite { target_username }
    → 200 {
            invited_code: str,        # 6-char Crockford code
            invite_url:   str,        # full https URL the invitee can open
            target_state: 'in_game' | 'idle' | 'offline',
            delivered:    bool,       # always false today; reserved for
                                       # the eventual push channel
          }
    → 404 user_not_found
    → 403 cannot_invite_blocker      # target has us blocked
    → 400 cannot_target_self

The endpoint creates a fresh multiplayer game with the caller as host
(host picks Black/X by default — same as the modal's default) so the
invitee has a real game to join. The invite is single-use; if the
invitee never joins, the standard 15-minute lazy-expiry kicks in.
"""

from __future__ import annotations

import asyncpg
from fastapi import APIRouter, Depends, HTTPException, status
from pydantic import BaseModel, Field

from app.config import settings
from app.database import get_pool
from app.multiplayer.codes import new_code
from app.security import get_current_user

router = APIRouter(prefix="/chat", tags=["chat"])


class InviteRequest(BaseModel):
    target_username: str = Field(min_length=2, max_length=30)


class InviteResponse(BaseModel):
    invited_code: str
    invite_url: str
    target_state: str  # 'in_game' | 'idle' | 'offline'
    delivered: bool


def _invite_url(code: str) -> str:
    """Build the public invite URL — same logic as the multiplayer router.

    Kept inline (rather than imported) to avoid coupling the chat router
    to multiplayer.py's internals; the duplicate is small and changes to
    the URL shape will affect both call sites symmetrically.
    """
    domain = settings.effective_domain
    scheme = (
        "http" if domain.startswith("localhost") or domain.startswith("127.") else "https"
    )
    return f"{scheme}://{domain}/play/{code}"


async def _target_state(conn: asyncpg.Connection, target_id: str) -> str:
    """'in_game' if the target has a waiting/in_progress multiplayer row,
    otherwise 'idle'. We don't have a presence layer yet — when we do,
    the 'offline' branch will look at last_seen_at < now() - 60s.
    """
    row = await conn.fetchrow(
        """
        SELECT 1 FROM multiplayer_games
        WHERE state IN ('waiting', 'in_progress')
          AND (host_user_id = $1::uuid OR guest_user_id = $1::uuid)
        LIMIT 1
        """,
        target_id,
    )
    return "in_game" if row is not None else "idle"


@router.post("/invite", response_model=InviteResponse)
async def invite(
    body: InviteRequest,
    user: dict = Depends(get_current_user),
    pool=Depends(get_pool),
) -> InviteResponse:
    caller_id = str(user["id"])
    async with pool.acquire() as conn:
        target = await conn.fetchrow(
            "SELECT id, username FROM users WHERE lower(username) = lower($1)",
            body.target_username,
        )
        if target is None:
            raise HTTPException(status.HTTP_404_NOT_FOUND, "user_not_found")
        if str(target["id"]) == caller_id:
            raise HTTPException(status.HTTP_400_BAD_REQUEST, "cannot_target_self")

        # Refuse if the target has blocked the caller. This is a proper
        # 403 (not a silent no-op) so the inviter learns why their invite
        # didn't go through.
        blocked = await conn.fetchrow(
            """
            SELECT 1 FROM blocks
            WHERE blocker_id = $1::uuid AND blocked_id = $2::uuid
            """,
            str(target["id"]),
            caller_id,
        )
        if blocked is not None:
            raise HTTPException(status.HTTP_403_FORBIDDEN, "cannot_invite_blocker")

        target_state = await _target_state(conn, str(target["id"]))

        # Allocate a unique code. Same pattern as multiplayer.py — bounded
        # retry loop wrapped in a per-attempt savepoint so a stray
        # collision doesn't poison the surrounding transaction.
        code: str | None = None
        last_exc: Exception | None = None
        for _ in range(8):
            candidate = new_code()
            try:
                async with conn.transaction():
                    await conn.execute(
                        """
                        INSERT INTO multiplayer_games
                            (code, host_user_id, host_color, board_size,
                             color_chosen_by)
                        VALUES ($1, $2::uuid, 'X', 15, 'host')
                        """,
                        candidate,
                        caller_id,
                    )
                code = candidate
                break
            except asyncpg.UniqueViolationError as exc:
                last_exc = exc
                continue
        if code is None:
            raise HTTPException(
                status.HTTP_503_SERVICE_UNAVAILABLE,
                f"failed_to_allocate_code: {last_exc}",
            )

    return InviteResponse(
        invited_code=code,
        invite_url=_invite_url(code),
        target_state=target_state,
        # `delivered` will become true once we add the push/notification
        # layer — for now the inviter copies + sends the URL out-of-band
        # (or the invitee opens the chat tab and gets a poll-based hint).
        delivered=False,
    )
