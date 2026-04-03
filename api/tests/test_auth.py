import pytest
from httpx import AsyncClient


@pytest.mark.asyncio
async def test_signup_success(client: AsyncClient):
    resp = await client.post(
        "/auth/signup",
        json={
            "username": "newuser",
            "password": "pass1234",
        },
    )
    assert resp.status_code == 200
    data = resp.json()
    assert data["username"] == "newuser"
    assert "access_token" in data
    assert data["token_type"] == "bearer"


@pytest.mark.asyncio
async def test_signup_with_email(client: AsyncClient):
    resp = await client.post(
        "/auth/signup",
        json={
            "username": "emailuser",
            "password": "pass1234",
            "email": "user@example.com",
        },
    )
    assert resp.status_code == 200
    assert resp.json()["username"] == "emailuser"


@pytest.mark.asyncio
async def test_signup_duplicate_username(client: AsyncClient):
    await client.post(
        "/auth/signup",
        json={
            "username": "taken",
            "password": "pass1234",
        },
    )
    resp = await client.post(
        "/auth/signup",
        json={
            "username": "taken",
            "password": "different",
        },
    )
    assert resp.status_code == 409
    assert "already taken" in resp.json()["detail"]


@pytest.mark.asyncio
async def test_signup_short_password(client: AsyncClient):
    resp = await client.post(
        "/auth/signup",
        json={
            "username": "shortpw",
            "password": "abc",
        },
    )
    assert resp.status_code == 422


@pytest.mark.asyncio
async def test_signup_short_username(client: AsyncClient):
    resp = await client.post(
        "/auth/signup",
        json={
            "username": "a",
            "password": "pass1234",
        },
    )
    assert resp.status_code == 422


@pytest.mark.asyncio
async def test_login_success(client: AsyncClient):
    await client.post(
        "/auth/signup",
        json={
            "username": "logintest",
            "password": "pass1234",
        },
    )
    resp = await client.post(
        "/auth/login",
        json={
            "username": "logintest",
            "password": "pass1234",
        },
    )
    assert resp.status_code == 200
    data = resp.json()
    assert data["username"] == "logintest"
    assert "access_token" in data


@pytest.mark.asyncio
async def test_login_case_insensitive_username(client: AsyncClient):
    await client.post(
        "/auth/signup",
        json={
            "username": "CaseUser",
            "password": "pass1234",
        },
    )
    resp = await client.post(
        "/auth/login",
        json={
            "username": "caseuser",
            "password": "pass1234",
        },
    )
    assert resp.status_code == 200


@pytest.mark.asyncio
async def test_login_wrong_password(client: AsyncClient):
    await client.post(
        "/auth/signup",
        json={
            "username": "wrongpw",
            "password": "pass1234",
        },
    )
    resp = await client.post(
        "/auth/login",
        json={
            "username": "wrongpw",
            "password": "wrongpass",
        },
    )
    assert resp.status_code == 401


@pytest.mark.asyncio
async def test_login_nonexistent_user(client: AsyncClient):
    resp = await client.post(
        "/auth/login",
        json={
            "username": "ghost",
            "password": "pass1234",
        },
    )
    assert resp.status_code == 401


@pytest.mark.asyncio
async def test_password_reset_request(client: AsyncClient):
    await client.post(
        "/auth/signup",
        json={
            "username": "resetuser",
            "password": "pass1234",
            "email": "reset@example.com",
        },
    )
    resp = await client.post(
        "/auth/password-reset",
        json={
            "email": "reset@example.com",
        },
    )
    assert resp.status_code == 200
    # Always returns success (no email enumeration)
    assert "email" in resp.json()["message"].lower() or "sent" in resp.json()["message"].lower()


@pytest.mark.asyncio
async def test_password_reset_nonexistent_email(client: AsyncClient):
    resp = await client.post(
        "/auth/password-reset",
        json={
            "email": "nobody@example.com",
        },
    )
    # Should still return 200 (no enumeration)
    assert resp.status_code == 200


@pytest.mark.asyncio
async def test_password_reset_confirm_invalid_token(client: AsyncClient):
    resp = await client.post(
        "/auth/password-reset/confirm",
        json={
            "token": "invalid-token",
            "new_password": "newpass123",
        },
    )
    assert resp.status_code == 400
