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
    → 403 cannot_invite_blocker         # target has blocked the caller
    → 429 {error, retry_at}             # caller hit the rolling-window cap
    → 400 cannot_target_self

Spam containment: per-caller rolling-window rate limit on invites.
A caller may send at most `INVITE_HOURLY_CAP` invites in any 1-hour
window and `INVITE_DAILY_CAP` in any 24-hour window. Counts include
all `multiplayer_games` rows where `host_user_id = caller AND
created_via = 'invite'` regardless of state — joining or expiring an
invite does NOT free a quota slot. Modal-created games (`created_via
= 'modal'`) are not counted.

When the cap is hit, the endpoint returns HTTP 429 with a structured
detail `{"error": "Your have reached invite maximum for this period.",
"retry_at": <ISO timestamp>}`. `retry_at` is the earliest moment at
which the next invite will succeed — the later of (oldest-in-hour
+ 1h) and (oldest-in-day + 24h), whichever cap is currently violated.

The endpoint creates a fresh multiplayer game with the caller as host
(host picks Black/X by default — same as the modal's default) so the
invitee has a real game to join. The invite is single-use; if the
invitee never joins, the standard 15-minute lazy-expiry kicks in.
"""

from __future__ import annotations

from datetime import UTC, datetime, timedelta
from typing import Literal

import asyncpg
from fastapi import APIRouter, Depends, HTTPException, status
from pydantic import BaseModel, Field

from app.database import get_pool
from app.multiplayer import allocate_game, game_invite_url
from app.security import get_current_user

router = APIRouter(prefix="/chat", tags=["chat"])

TargetState = Literal["in_game", "idle", "offline"]

# Rolling-window invite limits, per caller. Both windows are checked
# on every invite attempt; the most-restrictive cap wins.
INVITE_HOURLY_CAP = 7
INVITE_DAILY_CAP = 15

# Required verbatim by the spec — frontend pattern-matches on this
# string in the chat panel error caption. Do not reword without
# updating the spec.
RATE_LIMIT_ERROR = "Your have reached invite maximum for this period."


class InviteRequest(BaseModel):
    target_username: str = Field(min_length=2, max_length=30)


class InviteResponse(BaseModel):
    invited_code: str
    invite_url: str
    target_state: TargetState
    delivered: bool


async def _target_state(conn: asyncpg.Connection, target_id: str) -> TargetState:
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


async def _check_invite_rate_limit(
    conn: asyncpg.Connection, host_id: str
) -> datetime | None:
    """Return None if the caller may send another invite.

    Otherwise return the earliest UTC timestamp at which the next
    invite will succeed. Both the hourly and daily caps are checked;
    the later (most-restrictive) retry time wins.

    Implementation: pull up to DAILY_CAP recent invite created_at
    timestamps, newest first. If we have DAILY_CAP rows, the oldest
    in that batch must roll out of the 24h window before the next
    invite can land — that gives the daily retry. Same logic against
    the subset within the last hour for the hourly retry.
    """
    rows = await conn.fetch(
        """
        SELECT created_at
        FROM multiplayer_games
        WHERE host_user_id = $1::uuid
          AND created_via = 'invite'
          AND created_at > NOW() - INTERVAL '24 hours'
        ORDER BY created_at DESC
        LIMIT $2
        """,
        host_id,
        INVITE_DAILY_CAP,
    )
    now = datetime.now(UTC)

    daily_retry: datetime | None = None
    if len(rows) >= INVITE_DAILY_CAP:
        # rows[-1] is the DAILY_CAPth most recent (oldest in our batch).
        # It must age past 24h for the count to drop to DAILY_CAP - 1.
        daily_retry = rows[-1]["created_at"] + timedelta(hours=24)

    one_hour_ago = now - timedelta(hours=1)
    in_hour = [r["created_at"] for r in rows if r["created_at"] > one_hour_ago]
    hourly_retry: datetime | None = None
    if len(in_hour) >= INVITE_HOURLY_CAP:
        hourly_retry = in_hour[-1] + timedelta(hours=1)

    candidates = [t for t in (daily_retry, hourly_retry) if t is not None]
    return max(candidates) if candidates else None


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
        target_id = str(target["id"])
        if target_id == caller_id:
            raise HTTPException(status.HTTP_400_BAD_REQUEST, "cannot_target_self")

        # Refuse if the target has blocked the caller. This is a proper
        # 403 (not a silent no-op) so the inviter learns why their invite
        # didn't go through.
        blocked = await conn.fetchrow(
            """
            SELECT 1 FROM blocks
            WHERE blocker_id = $1::uuid AND blocked_id = $2::uuid
            """,
            target_id,
            caller_id,
        )
        if blocked is not None:
            raise HTTPException(status.HTTP_403_FORBIDDEN, "cannot_invite_blocker")

        # Rolling-window rate limit. retry_at is the earliest moment
        # the next invite will land; the frontend formats it for the
        # user in the chat error caption.
        retry_at = await _check_invite_rate_limit(conn, caller_id)
        if retry_at is not None:
            raise HTTPException(
                status.HTTP_429_TOO_MANY_REQUESTS,
                {
                    "error": RATE_LIMIT_ERROR,
                    "retry_at": retry_at.isoformat(),
                },
            )

        target_state = await _target_state(conn, target_id)

        try:
            row = await allocate_game(
                conn,
                host_user_id=caller_id,
                created_via="invite",
            )
        except RuntimeError as exc:
            raise HTTPException(
                status.HTTP_503_SERVICE_UNAVAILABLE, str(exc)
            ) from exc
        code = row["code"]

    return InviteResponse(
        invited_code=code,
        invite_url=game_invite_url(code),
        target_state=target_state,
        # `delivered` will become true once we add the push/notification
        # layer — for now the inviter copies + sends the URL out-of-band
        # (or the invitee opens the chat tab and gets a poll-based hint).
        delivered=False,
    )
