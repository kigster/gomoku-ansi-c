"""Tests for CurrentSession model, get_session dependency, and middleware integration."""

from unittest.mock import MagicMock

import pytest
from httpx import AsyncClient

from app.security import create_token
from app.session import CurrentSession, get_session

# ─── Unit: CurrentSession model ──────────────────────────────────────────────


class TestCurrentSession:
    def test_defaults_are_empty(self):
        session = CurrentSession()
        assert session.user_id is None
        assert session.username is None
        assert session.jwt_payload is None

    def test_is_authenticated_when_user_id_present(self):
        session = CurrentSession(user_id="abc-123", username="alice")
        assert session.is_authenticated is True

    def test_not_authenticated_when_empty(self):
        session = CurrentSession()
        assert session.is_authenticated is False

    def test_not_authenticated_with_username_only(self):
        session = CurrentSession(username="alice")
        assert session.is_authenticated is False

    def test_payload_round_trips(self):
        payload = {"sub": "id-1", "username": "bob", "exp": 9999999999}
        session = CurrentSession(
            user_id=payload["sub"],
            username=payload["username"],
            jwt_payload=payload,
        )
        jp = session.jwt_payload
        assert jp is not None
        assert jp["sub"] == "id-1"
        assert jp["exp"] == 9999999999


# ─── Unit: get_session dependency ────────────────────────────────────────────


class TestGetSession:
    def test_returns_empty_session_when_no_session_on_state(self):
        request = MagicMock()
        request.state = MagicMock(spec=[])
        session = get_session(request)
        assert session == CurrentSession()

    def test_returns_session_from_request_state(self):
        expected = CurrentSession(user_id="u1", username="alice")
        request = MagicMock()
        request.state.current_session = expected
        assert get_session(request) is expected


# ─── Integration: middleware populates session ───────────────────────────────


@pytest.mark.asyncio
async def test_authenticated_request_returns_user(client: AsyncClient):
    """Full stack: signup → use token → get_current_user resolves via session."""
    resp = await client.post(
        "/auth/signup",
        json={"username": "session_user", "password": "pass1234"},
    )
    token = resp.json()["access_token"]

    resp = await client.get("/user/me", headers={"Authorization": f"Bearer {token}"})
    assert resp.status_code == 200
    assert resp.json()["username"] == "session_user"


@pytest.mark.asyncio
async def test_no_token_returns_401(client: AsyncClient):
    """Request without Authorization header gets 401."""
    resp = await client.get("/user/me")
    assert resp.status_code == 401


@pytest.mark.asyncio
async def test_invalid_token_returns_401(client: AsyncClient):
    """Garbage token gets 401 with meaningful error."""
    resp = await client.get("/user/me", headers={"Authorization": "Bearer garbage.token.here"})
    assert resp.status_code == 401


@pytest.mark.asyncio
async def test_expired_token_returns_401(client: AsyncClient, registered_user):
    """An expired token signed with the correct secret should return 'Token expired'."""
    from datetime import UTC, datetime, timedelta

    import jwt

    from app.config import settings

    payload = {
        "sub": "00000000-0000-0000-0000-000000000000",
        "username": "someone",
        "exp": datetime.now(UTC) - timedelta(hours=1),
    }
    expired = jwt.encode(payload, settings.jwt_secret, algorithm=settings.jwt_algorithm)

    resp = await client.get("/user/me", headers={"Authorization": f"Bearer {expired}"})
    assert resp.status_code == 401
    assert "expired" in resp.json()["detail"].lower()


@pytest.mark.asyncio
async def test_token_without_sub_returns_401(client: AsyncClient):
    """A valid JWT that has no 'sub' claim should fail with 'Invalid token payload'."""
    import jwt

    from app.config import settings

    payload = {"username": "nosub", "exp": 9999999999}
    token = jwt.encode(payload, settings.jwt_secret, algorithm=settings.jwt_algorithm)

    resp = await client.get("/user/me", headers={"Authorization": f"Bearer {token}"})
    assert resp.status_code == 401


@pytest.mark.asyncio
async def test_token_for_deleted_user_returns_401(client: AsyncClient):
    """Valid JWT but user no longer in DB → 401."""
    # Create a syntactically valid token for a nonexistent user_id
    token = create_token("00000000-0000-0000-0000-000000000000", "ghost")

    resp = await client.get("/user/me", headers={"Authorization": f"Bearer {token}"})
    assert resp.status_code == 401


@pytest.mark.asyncio
async def test_health_has_no_session_requirement(client: AsyncClient):
    """Unauthenticated endpoints still work — middleware sets empty session."""
    resp = await client.get("/health")
    assert resp.status_code == 200
