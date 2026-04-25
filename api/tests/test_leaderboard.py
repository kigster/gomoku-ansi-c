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
    assert entries[0]["username"] == "testplayer"
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
async def test_leaderboard_dedupes_to_highest_score(client: AsyncClient, auth_headers):
    # Game 1: radius=3, default times → some score
    g1 = {**SAMPLE_GAME_JSON, "radius": 3}
    r1 = await client.post("/game/save", headers=auth_headers, json={"game_json": g1})
    assert r1.status_code == 200, r1.text
    score_low = r1.json()["score"]

    # Game 2: radius=5 → strictly higher score for the same player
    g2 = {**SAMPLE_GAME_JSON, "radius": 5}
    r2 = await client.post("/game/save", headers=auth_headers, json={"game_json": g2})
    assert r2.status_code == 200, r2.text
    score_high = r2.json()["score"]

    # Game 3: radius=2 → lower score for the same player
    g3 = {**SAMPLE_GAME_JSON, "radius": 2}
    r3 = await client.post("/game/save", headers=auth_headers, json={"game_json": g3})
    assert r3.status_code == 200, r3.text

    assert score_high > score_low

    resp = await client.get("/leaderboard")
    assert resp.status_code == 200
    entries = resp.json()["entries"]
    assert len(entries) == 1, f"expected 1 entry per player, got {len(entries)}"
    assert entries[0]["username"] == "testplayer"
    assert entries[0]["score"] == score_high
    assert entries[0]["radius"] == 5


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
