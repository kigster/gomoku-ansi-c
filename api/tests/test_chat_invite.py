"""Tests for /chat/invite — the only chat endpoint that ships in this
PR. Covers presence detection (in_game vs idle), code allocation,
self-target rejection, blocked-target rejection, and 404 on unknown
target.
"""

from __future__ import annotations

import pytest
from httpx import AsyncClient


@pytest.mark.asyncio
async def test_invite_idle_target_returns_idle_state_and_code(
    client: AsyncClient, make_user
):
    alice = await make_user("alice")
    await make_user("bob")
    resp = await client.post(
        "/chat/invite",
        headers=alice["headers"],
        json={"target_username": "bob"},
    )
    assert resp.status_code == 200, resp.text
    body = resp.json()
    assert body["target_state"] == "idle"
    assert body["delivered"] is False
    assert len(body["invited_code"]) == 6
    assert body["invite_url"].endswith(f"/play/{body['invited_code']}")


@pytest.mark.asyncio
async def test_invite_in_game_target_returns_in_game(client: AsyncClient, make_user):
    alice = await make_user("alice")
    bob = await make_user("bob")
    carol = await make_user("carol")

    # Bob is already busy in a game with Carol.
    new_resp = await client.post(
        "/multiplayer/new",
        headers=bob["headers"],
        json={"host_color": "X", "board_size": 15},
    )
    assert new_resp.status_code == 200
    code = new_resp.json()["code"]
    join_resp = await client.post(
        f"/multiplayer/{code}/join",
        headers=carol["headers"],
        json={},
    )
    assert join_resp.status_code == 200

    # Alice tries to invite Bob — server reports he's in_game.
    resp = await client.post(
        "/chat/invite",
        headers=alice["headers"],
        json={"target_username": "bob"},
    )
    assert resp.status_code == 200
    assert resp.json()["target_state"] == "in_game"


@pytest.mark.asyncio
async def test_invite_unknown_target_404(client: AsyncClient, auth_headers):
    resp = await client.post(
        "/chat/invite",
        headers=auth_headers,
        json={"target_username": "ghost-user"},
    )
    assert resp.status_code == 404


@pytest.mark.asyncio
async def test_invite_self_400(client: AsyncClient, registered_user):
    name, token = registered_user
    resp = await client.post(
        "/chat/invite",
        headers={"Authorization": f"Bearer {token}"},
        json={"target_username": name},
    )
    assert resp.status_code == 400


@pytest.mark.asyncio
async def test_invite_when_blocked_returns_403(client: AsyncClient, make_user):
    alice = await make_user("alice")
    bob = await make_user("bob")
    # Bob blocks Alice → Alice can no longer invite Bob.
    block_resp = await client.post(
        "/social/block",
        headers=bob["headers"],
        json={"target_username": "alice"},
    )
    assert block_resp.status_code == 200
    invite_resp = await client.post(
        "/chat/invite",
        headers=alice["headers"],
        json={"target_username": "bob"},
    )
    assert invite_resp.status_code == 403


@pytest.mark.asyncio
async def test_invite_creates_real_game_target_can_join(
    client: AsyncClient, make_user
):
    alice = await make_user("alice")
    bob = await make_user("bob")
    invite_resp = await client.post(
        "/chat/invite",
        headers=alice["headers"],
        json={"target_username": "bob"},
    )
    assert invite_resp.status_code == 200
    code = invite_resp.json()["invited_code"]

    # Bob should be able to walk up to that code and join — proving the
    # invite isn't a fake/placeholder.
    join_resp = await client.post(
        f"/multiplayer/{code}/join",
        headers=bob["headers"],
        json={},
    )
    assert join_resp.status_code == 200
    assert join_resp.json()["state"] == "in_progress"
