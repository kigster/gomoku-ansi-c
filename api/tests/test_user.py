import pytest
from httpx import AsyncClient

from tests.conftest import SAMPLE_GAME_JSON


@pytest.mark.asyncio
async def test_user_me_authenticated(client: AsyncClient, auth_headers, registered_user):
    username, _ = registered_user
    resp = await client.get("/user/me", headers=auth_headers)
    assert resp.status_code == 200
    data = resp.json()
    assert data["username"] == username
    assert data["email"] == "test@example.com"
    assert len(data["id"]) == 36  # UUID
    assert data["personal_best"] is None


@pytest.mark.asyncio
async def test_user_me_unauthenticated(client: AsyncClient):
    resp = await client.get("/user/me")
    assert resp.status_code == 401


@pytest.mark.asyncio
async def test_user_me_invalid_token(client: AsyncClient):
    resp = await client.get("/user/me", headers={"Authorization": "Bearer invalid.token.here"})
    assert resp.status_code == 401


@pytest.mark.asyncio
async def test_user_me_with_personal_best(client: AsyncClient, auth_headers):
    # Save a winning game
    await client.post(
        "/game/save",
        headers=auth_headers,
        json={
            "game_json": SAMPLE_GAME_JSON,
        },
    )

    resp = await client.get("/user/me", headers=auth_headers)
    assert resp.status_code == 200
    data = resp.json()
    best = data["personal_best"]
    assert best is not None
    assert best["score"] > 0
    assert best["depth"] == 5
    assert best["radius"] == 3
