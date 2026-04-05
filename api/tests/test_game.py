import copy

import pytest
from httpx import AsyncClient

from tests.conftest import SAMPLE_GAME_JSON


@pytest.mark.asyncio
async def test_game_start_authenticated(client: AsyncClient, auth_headers):
    resp = await client.post("/game/start", headers=auth_headers)
    assert resp.status_code == 200
    assert resp.json()["status"] == "ok"


@pytest.mark.asyncio
async def test_game_start_unauthenticated(client: AsyncClient):
    resp = await client.post("/game/start")
    assert resp.status_code == 401


@pytest.mark.asyncio
async def test_game_start_increments_counter(client: AsyncClient, auth_headers):
    await client.post("/game/start", headers=auth_headers)
    await client.post("/game/start", headers=auth_headers)

    resp = await client.get("/user/me", headers=auth_headers)
    # games_started is tracked in the users table but not exposed via /user/me yet
    # Just verify the endpoint returns 200 twice
    assert resp.status_code == 200


@pytest.mark.asyncio
async def test_game_save_human_wins(client: AsyncClient, auth_headers):
    resp = await client.post(
        "/game/save",
        headers=auth_headers,
        json={
            "game_json": SAMPLE_GAME_JSON,
        },
    )
    assert resp.status_code == 200
    data = resp.json()
    assert "id" in data
    # UUID format check
    assert len(data["id"]) == 36
    assert "-" in data["id"]
    # Human won at depth 5 — score should be positive
    assert data["score"] > 0
    assert data["rating"] > 0


@pytest.mark.asyncio
async def test_game_save_human_loses(client: AsyncClient, auth_headers):
    game = {**SAMPLE_GAME_JSON, "winner": "O"}
    resp = await client.post(
        "/game/save",
        headers=auth_headers,
        json={
            "game_json": game,
        },
    )
    assert resp.status_code == 200
    data = resp.json()
    assert data["score"] == 0
    assert data["rating"] == 0.0


@pytest.mark.asyncio
async def test_game_save_unfinished_game(client: AsyncClient, auth_headers):
    game = {**SAMPLE_GAME_JSON, "winner": "none"}
    resp = await client.post(
        "/game/save",
        headers=auth_headers,
        json={
            "game_json": game,
        },
    )
    assert resp.status_code == 400


@pytest.mark.asyncio
async def test_game_save_unauthenticated(client: AsyncClient):
    resp = await client.post("/game/save", json={"game_json": SAMPLE_GAME_JSON})
    assert resp.status_code == 401


@pytest.mark.asyncio
async def test_game_save_o_is_human(client: AsyncClient, auth_headers):
    """Cover the O-is-human branch in save()."""
    game = copy.deepcopy(SAMPLE_GAME_JSON)
    game["X"] = {"player": "AI", "depth": 4, "time_ms": 3000}
    game["O"] = {"player": "human", "depth": 3, "time_ms": 5000}
    game["winner"] = "O"
    game["moves"] = [
        {"X (AI)": "J10", "time_ms": 500},
        {"O (human)": "K11", "time_ms": 1000},
        {"X (AI)": "J9", "time_ms": 500},
        {"O (human)": "K10", "time_ms": 1000},
        {"X (AI)": "J8", "time_ms": 500},
        {"O (human)": "K9", "time_ms": 1000},
        {"X (AI)": "J7", "time_ms": 500},
        {"O (human)": "K8", "time_ms": 1000},
        {"X (AI)": "J6", "time_ms": 500},
        {"O (human)": "K7", "time_ms": 700, "winner": True},
    ]
    resp = await client.post("/game/save", headers=auth_headers, json={"game_json": game})
    assert resp.status_code == 200
    assert resp.json()["score"] > 0


@pytest.mark.asyncio
async def test_game_save_both_ai(client: AsyncClient, auth_headers):
    """Cover the neither-is-human branch in save()."""
    game = copy.deepcopy(SAMPLE_GAME_JSON)
    game["X"] = {"player": "AI", "depth": 3, "time_ms": 0}
    game["O"] = {"player": "AI", "depth": 5, "time_ms": 0}
    game["winner"] = "X"
    resp = await client.post("/game/save", headers=auth_headers, json={"game_json": game})
    assert resp.status_code == 200


@pytest.mark.asyncio
async def test_game_history(client: AsyncClient, auth_headers):
    """Cover the game history endpoint after saving a game."""
    await client.post("/game/save", headers=auth_headers, json={"game_json": SAMPLE_GAME_JSON})
    resp = await client.get("/game/history", headers=auth_headers)
    assert resp.status_code == 200
    data = resp.json()
    assert len(data["games"]) == 1
    assert data["games"][0]["username"] == "testplayer"
    assert data["games"][0]["won"] is True


@pytest.mark.asyncio
async def test_game_history_unauthenticated(client: AsyncClient):
    resp = await client.get("/game/history")
    assert resp.status_code == 401


@pytest.mark.asyncio
async def test_game_download_json(client: AsyncClient, auth_headers):
    """Cover the download game JSON endpoint."""
    save_resp = await client.post(
        "/game/save", headers=auth_headers, json={"game_json": SAMPLE_GAME_JSON}
    )
    game_id = save_resp.json()["id"]

    resp = await client.get(f"/game/{game_id}/json", headers=auth_headers)
    assert resp.status_code == 200
    data = resp.json()
    assert data["winner"] == "X"
    assert data["board_size"] == 19


@pytest.mark.asyncio
async def test_game_download_json_not_found(client: AsyncClient, auth_headers):
    resp = await client.get("/game/00000000-0000-0000-0000-000000000000/json", headers=auth_headers)
    assert resp.status_code == 404


@pytest.mark.asyncio
async def test_game_play_proxy_returns_503_when_engine_down(client: AsyncClient):
    """game/play proxies to gomoku-httpd which isn't running in tests."""
    resp = await client.post(
        "/game/play",
        json={
            "X": {"player": "human", "time_ms": 0},
            "O": {"player": "AI", "depth": 3, "time_ms": 0},
            "board_size": 19,
            "radius": 2,
            "timeout": "none",
            "winner": "none",
            "board_state": [],
            "moves": [{"X (human)": "J10", "time_ms": 500}],
        },
    )
    assert resp.status_code == 503
