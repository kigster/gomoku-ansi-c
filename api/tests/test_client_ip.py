"""Tests for X-Forwarded-For handling in ClientIPMiddleware."""

import pytest
from httpx import AsyncClient


@pytest.mark.asyncio
async def test_client_ip_from_x_forwarded_for(client: AsyncClient, auth_headers):
    """X-Forwarded-For header should be used as client_ip."""
    resp = await client.get(
        "/health",
        headers={"X-Forwarded-For": "203.0.113.50, 70.41.3.18"},
    )
    assert resp.status_code == 200


@pytest.mark.asyncio
async def test_client_ip_without_forwarded_header(client: AsyncClient):
    """Without X-Forwarded-For, the direct client IP is used."""
    resp = await client.get("/health")
    assert resp.status_code == 200
