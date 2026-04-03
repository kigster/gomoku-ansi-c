import os

from dotenv import load_dotenv

load_dotenv()

import asyncpg
import httpx
import pytest
from httpx import ASGITransport, AsyncClient

# Derive DSNs from DATABASE_URL (.env or CI environment).
_db_url = os.environ.get("DATABASE_URL", "postgresql://postgres@localhost:5432/gomoku_test")
_admin_dsn = _db_url.rsplit("/", 1)[0] + "/postgres"
TEST_DSN = _db_url

# Set test env BEFORE any app imports
os.environ.setdefault("DATABASE_URL", TEST_DSN)
os.environ.setdefault("JWT_SECRET", "test-secret-key-for-unit-tests-only!!")
os.environ.setdefault("GOMOKU_HTTPD_URL", "http://localhost:1")

from app.database import create_pool
from app.main import app, fastapi_app

_initialized = False


async def _ensure_initialized():
    global _initialized
    if _initialized:
        return
    _initialized = True

    # Create test database if needed
    conn = await asyncpg.connect(_admin_dsn)
    try:
        db_name = TEST_DSN.rsplit("/", 1)[-1].split("?")[0]
        exists = await conn.fetchval(
            "SELECT 1 FROM pg_database WHERE datname = $1", db_name
        )
        if not exists:
            await conn.execute(f'CREATE DATABASE "{db_name}"')
    finally:
        await conn.close()

    # Apply schema if needed
    conn = await asyncpg.connect(TEST_DSN)
    try:
        has_schema = await conn.fetchval(
            "SELECT 1 FROM information_schema.tables WHERE table_name = 'users'"
        )
        if not has_schema:
            schema_path = os.path.join(
                os.path.dirname(__file__), "..", "..", "iac", "cloud_sql", "setup.sql"
            )
            with open(schema_path) as f:
                await conn.execute(f.read())
    finally:
        await conn.close()

    # Initialize app state on the FastAPI instance (not the ASGI wrapper)
    fastapi_app.state.db_pool = await create_pool()
    fastapi_app.state.httpx_client = httpx.AsyncClient(
        base_url="http://localhost:1",
        timeout=httpx.Timeout(5.0),
    )


@pytest.fixture
async def client():
    """Async HTTP client wired to the FastAPI app."""
    await _ensure_initialized()
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as ac:
        yield ac
    conn = await asyncpg.connect(TEST_DSN)
    try:
        await conn.execute("TRUNCATE games, password_reset_tokens, users CASCADE")
    finally:
        await conn.close()


@pytest.fixture
async def registered_user(client: AsyncClient):
    """Create a user and return (username, token)."""
    resp = await client.post("/auth/signup", json={
        "username": "testplayer",
        "password": "testpass123",
        "email": "test@example.com",
    })
    assert resp.status_code == 200, resp.text
    data = resp.json()
    return data["username"], data["access_token"]


@pytest.fixture
def auth_headers(registered_user):
    """Authorization headers for authenticated requests."""
    _, token = registered_user
    return {"Authorization": f"Bearer {token}"}


SAMPLE_GAME_JSON = {
    "X": {"player": "human", "depth": 3, "time_ms": 5000},
    "O": {"player": "AI", "depth": 5, "time_ms": 3000},
    "board_size": 19,
    "radius": 3,
    "timeout": "none",
    "winner": "X",
    "board_state": [],
    "moves": [
        {"X (human)": "J9", "time_ms": 1000},
        {"O (AI)": "K10", "time_ms": 500},
        {"X (human)": "J8", "time_ms": 2000},
        {"O (AI)": "K9", "time_ms": 500},
        {"X (human)": "J7", "time_ms": 1500},
        {"O (AI)": "K8", "time_ms": 500},
        {"X (human)": "J6", "time_ms": 800},
        {"O (AI)": "K7", "time_ms": 500},
        {"X (human)": "J5", "time_ms": 700, "winner": True},
    ],
}
