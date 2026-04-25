"""Typed per-request session, populated by middleware from the JWT payload.

Usage in route handlers:
    from app.session import CurrentSession, get_session

    @router.get("/example")
    async def example(session: CurrentSession = Depends(get_session)):
        if session.user_id:
            ...
"""

from pydantic import BaseModel
from starlette.requests import Request


class CurrentSession(BaseModel):
    """Per-request session data decoded from the Bearer token."""

    user_id: str | None = None
    username: str | None = None
    jwt_payload: dict | None = None

    @property
    def is_authenticated(self) -> bool:
        return self.user_id is not None


def get_session(request: Request) -> CurrentSession:
    """FastAPI dependency — returns the typed session from request.state."""
    return getattr(request.state, "current_session", CurrentSession())
