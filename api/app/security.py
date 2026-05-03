from datetime import UTC, datetime, timedelta

import bcrypt
import jwt
from fastapi import Depends, HTTPException, Request, status
from fastapi.security import HTTPBearer

from app.config import settings
from app.database import get_pool
from app.session import get_session

bearer_scheme = HTTPBearer(auto_error=False)

# Presence-bump throttle: don't refresh users.last_seen_at more than
# once per this many seconds per user. See get_current_user.
PRESENCE_BUMP_THROTTLE_SEC = 10


def hash_password(password: str) -> str:
    return bcrypt.hashpw(password.encode(), bcrypt.gensalt()).decode()


def verify_password(password: str, hashed: str) -> bool:
    return bcrypt.checkpw(password.encode(), hashed.encode())


def create_token(user_id: str, username: str) -> str:
    payload = {
        "sub": user_id,
        "username": username,
        "exp": datetime.now(UTC) + timedelta(minutes=settings.jwt_expire_minutes),
    }
    return jwt.encode(payload, settings.jwt_secret, algorithm=settings.jwt_algorithm)


def decode_token(token: str) -> dict:
    try:
        return jwt.decode(token, settings.jwt_secret, algorithms=[settings.jwt_algorithm])
    except jwt.ExpiredSignatureError:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Token expired")
    except jwt.InvalidTokenError as e:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, f"Invalid token: {e}")


async def get_current_user(
    request: Request,
    pool=Depends(get_pool),
) -> dict:
    """Resolve the authenticated user. Uses the JWT already decoded by middleware."""
    session = get_session(request)
    if session.jwt_payload:
        payload = session.jwt_payload
    else:
        # Middleware didn't decode (no token or invalid) — try with proper error messages
        auth = request.headers.get("authorization", "")
        if not auth.startswith("Bearer "):
            raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Not authenticated")
        payload = decode_token(auth[7:])

    user_id = payload.get("sub")
    if user_id is None:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Invalid token payload")
    async with pool.acquire() as conn:
        row = await conn.fetchrow(
            """SELECT id, username, email, first_name, last_name,
                      created_at, last_logged_in_at, logins_count,
                      last_seen_at
               FROM users WHERE id = $1::uuid""",
            user_id,
        )
        if row is None:
            raise HTTPException(status.HTTP_401_UNAUTHORIZED, "User not found")
        # Throttled presence bump. An active player polls
        # /multiplayer/{code} every 300 ms; if we wrote on every call
        # we'd produce 3 dead tuples/sec/user (Postgres doesn't
        # deduplicate value-equal UPDATEs — a new row version is
        # created regardless). Throttling to one write per
        # PRESENCE_BUMP_THROTTLE_SEC collapses bursts into a single
        # row-version per window. The presence window is 60 s, so
        # 10-second resolution is plenty.
        last_seen = row["last_seen_at"]
        if (datetime.now(UTC) - last_seen).total_seconds() >= PRESENCE_BUMP_THROTTLE_SEC:
            await conn.execute(
                "UPDATE users SET last_seen_at = now() WHERE id = $1::uuid",
                user_id,
            )
    return dict(row)


async def get_optional_user(
    request: Request,
    pool=Depends(get_pool),
) -> dict | None:
    session = get_session(request)
    if not session.jwt_payload:
        auth = request.headers.get("authorization", "")
        if not auth.startswith("Bearer "):
            return None
    try:
        return await get_current_user(request, pool)
    except HTTPException:
        return None
