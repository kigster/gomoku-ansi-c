import os
import subprocess
from pathlib import Path

import asyncpg
import httpx
import pytest
from dotenv import load_dotenv
from httpx import ASGITransport, AsyncClient

# Load .env.ci if ENVIRONMENT=ci, otherwise .env (local dev)
_api_dir = Path(__file__).resolve().parent.parent
if os.environ.get("CI", "") != "":
    _env_file = _api_dir / ".env.ci"
else:
    _env_file = _api_dir / ".env"

if not _env_file.is_file():
    _env_file = _api_dir / ".env"

load_dotenv(_env_file, override=False)

os.environ.setdefault("JWT_SECRET", "test-secret-key-for-unit-tests-only!!")
os.environ.setdefault("GOMOKU_HTTPD_URL", "http://localhost:10000")

# Derive test DSN: append _test to the database name from DATABASE_URL
_base_url = os.environ.get("DATABASE_URL", "postgresql://postgres@localhost:5432/gomoku")
_parts = _base_url.rsplit("/", 1)
_base_db = _parts[-1].split("?")[0]
_test_db = _base_db if _base_db.endswith("_test") else f"{_base_db}_test"
TEST_DSN = f"{_parts[0]}/{_test_db}"
_admin_dsn = f"{_parts[0]}/postgres"

# Override DATABASE_URL so the app uses the test database
os.environ["DATABASE_URL"] = TEST_DSN

from app.database import create_pool  # noqa: E402
from app.main import app, fastapi_app  # noqa: E402

_initialized = False


async def _ensure_initialized():
    global _initialized
    if _initialized:
        return
    _initialized = True

    # Create test database if it doesn't exist
    conn = await asyncpg.connect(_admin_dsn)
    try:
        exists = await conn.fetchval("SELECT 1 FROM pg_database WHERE datname = $1", _test_db)
        if not exists:
            await conn.execute(f'CREATE DATABASE "{_test_db}"')
    finally:
        await conn.close()

    # Run Alembic migrations (upgrade to head)
    env = {**os.environ, "DATABASE_URL": TEST_DSN}
    subprocess.run(
        ["uv", "run", "alembic", "upgrade", "head"],
        cwd=str(_api_dir),
        env=env,
        check=True,
        capture_output=True,
    )

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
    resp = await client.post(
        "/auth/signup",
        json={
            "username": "testplayer",
            "password": "testpass123",
            "email": "test@example.com",
        },
    )
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
        {"X (human)": "J10", "time_ms": 1000},
        {"O (AI)": "K11", "time_ms": 500},
        {"X (human)": "J9", "time_ms": 2000},
        {"O (AI)": "K10", "time_ms": 500},
        {"X (human)": "J8", "time_ms": 1500},
        {"O (AI)": "K9", "time_ms": 500},
        {"X (human)": "J7", "time_ms": 800},
        {"O (AI)": "K8", "time_ms": 500},
        {"X (human)": "J6", "time_ms": 700, "winner": True},
    ],
}
