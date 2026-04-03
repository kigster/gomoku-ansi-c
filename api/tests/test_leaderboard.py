import pytest
from httpx import AsyncClient

from tests.conftest import SAMPLE_GAME_JSON


@pytest.mark.asyncio
async def test_leaderboard_empty(client: AsyncClient):
    resp = await client.get("/leaderboard")
    assert resp.status_code == 200
    assert resp.json()["entries"] == []


@pytest.mark.asyncio
async def test_leaderboard_with_scores(client: AsyncClient, auth_headers):
    # Save a winning game
    await client.post(
        "/game/save",
        headers=auth_headers,
        json={
            "game_json": SAMPLE_GAME_JSON,
        },
    )

    resp = await client.get("/leaderboard")
    assert resp.status_code == 200
    entries = resp.json()["entries"]
    assert len(entries) == 1
    assert entries[0]["player_name"] == "testplayer"
    assert entries[0]["score"] > 0
    assert entries[0]["rating"] > 0
    assert entries[0]["depth"] == 5


@pytest.mark.asyncio
async def test_leaderboard_excludes_losses(client: AsyncClient, auth_headers):
    # Save a losing game (score = 0)
    game = {**SAMPLE_GAME_JSON, "winner": "O"}
    await client.post(
        "/game/save",
        headers=auth_headers,
        json={
            "game_json": game,
        },
    )

    resp = await client.get("/leaderboard")
    assert resp.status_code == 200
    assert resp.json()["entries"] == []


@pytest.mark.asyncio
async def test_leaderboard_limit(client: AsyncClient):
    resp = await client.get("/leaderboard?limit=5")
    assert resp.status_code == 200


@pytest.mark.asyncio
async def test_leaderboard_invalid_limit(client: AsyncClient):
    resp = await client.get("/leaderboard?limit=0")
    assert resp.status_code == 422

    resp = await client.get("/leaderboard?limit=999")
    assert resp.status_code == 422
