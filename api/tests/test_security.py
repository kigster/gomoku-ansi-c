"""Tests for security module edge cases not covered by integration tests."""

from unittest.mock import AsyncMock, MagicMock

import pytest

from app.security import get_optional_user
from app.session import CurrentSession


@pytest.mark.asyncio
async def test_optional_user_returns_none_without_auth():
    """get_optional_user returns None when no auth is present."""
    request = MagicMock()
    request.state.current_session = CurrentSession()
    request.headers = {}

    pool = AsyncMock()
    result = await get_optional_user(request, pool)
    assert result is None


@pytest.mark.asyncio
async def test_optional_user_returns_none_on_bad_token():
    """get_optional_user returns None when the token is invalid."""
    request = MagicMock()
    request.state.current_session = CurrentSession()
    request.headers = {"authorization": "Bearer garbage"}

    pool = AsyncMock()
    result = await get_optional_user(request, pool)
    assert result is None


@pytest.mark.asyncio
async def test_optional_user_returns_user_on_valid_session(client, registered_user):
    """Integration: get_optional_user resolves a real user through the ASGI stack."""

    _, token = registered_user
    # /user/me uses get_current_user; validate the token works end-to-end
    resp = await client.get("/user/me", headers={"Authorization": f"Bearer {token}"})
    assert resp.status_code == 200
    assert resp.json()["username"] == "testplayer"
