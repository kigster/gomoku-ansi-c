"""Verify Alembic migrations apply and roll back cleanly."""

import os
import subprocess
from pathlib import Path

import asyncpg
import pytest

# Reuse the resolved test DSN from conftest. conftest already handles the
# pytest-xdist per-worker DB naming, ENVIRONMENT pinning, and any local
# overrides — duplicating that logic here previously caused orphan
# `gomoku_test_gwN_test` databases to be created.
from tests.conftest import TEST_DSN as _dsn  # noqa: E402
from tests.conftest import _admin_dsn, _test_db

_api_dir = Path(__file__).resolve().parent.parent


def _alembic(cmd: str, *args: str) -> subprocess.CompletedProcess:
    env = {**os.environ, "DATABASE_URL": _dsn}
    result = subprocess.run(
        ["uv", "run", "alembic", cmd, *args],
        cwd=str(_api_dir),
        env=env,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(f"alembic {cmd} failed:\n{result.stderr}")
    return result


@pytest.fixture
async def fresh_db():
    """Drop all tables and alembic_version so we start from scratch."""
    conn = await asyncpg.connect(_admin_dsn)
    try:
        exists = await conn.fetchval("SELECT 1 FROM pg_database WHERE datname = $1", _test_db)
        if not exists:
            await conn.execute(f'CREATE DATABASE "{_test_db}"')
    finally:
        await conn.close()

    conn = await asyncpg.connect(_dsn)
    try:
        await conn.execute("DROP MATERIALIZED VIEW IF EXISTS leaderboard CASCADE")
        await conn.execute("DROP VIEW IF EXISTS top_scores, games_pending_geo CASCADE")
        await conn.execute(
            "DROP FUNCTION IF EXISTS game_score(BOOLEAN, INT, INT, DOUBLE PRECISION) CASCADE"
        )
        await conn.execute("DROP FUNCTION IF EXISTS game_time_score(DOUBLE PRECISION) CASCADE")
        # Discover all user tables in the public schema and drop them. Doing
        # this dynamically means new migrations don't need to update a
        # hard-coded list (the previous static list missed multiplayer_games
        # and forced a defensive DROP inside that migration's upgrade()).
        rows = await conn.fetch("SELECT tablename FROM pg_tables WHERE schemaname = 'public'")
        table_names = [r["tablename"] for r in rows]
        if table_names:
            quoted = ", ".join(f'"{name}"' for name in table_names)
            await conn.execute(f"DROP TABLE IF EXISTS {quoted} CASCADE")
        await conn.execute("DROP TABLE IF EXISTS alembic_version CASCADE")
    finally:
        await conn.close()

    yield

    # Restore to head for other tests
    _alembic("upgrade", "head")


async def _relation_exists(name: str) -> bool:
    """Check if a table, view, or materialized view exists."""
    conn = await asyncpg.connect(_dsn)
    try:
        result = await conn.fetchval(
            "SELECT 1 FROM pg_class c "
            "JOIN pg_namespace n ON n.oid = c.relnamespace "
            "WHERE c.relname = $1 AND n.nspname = 'public'",
            name,
        )
        return result is not None
    finally:
        await conn.close()


async def _matview_exists(name: str) -> bool:
    """Check if a materialized view exists."""
    conn = await asyncpg.connect(_dsn)
    try:
        result = await conn.fetchval("SELECT 1 FROM pg_matviews WHERE matviewname = $1", name)
        return result is not None
    finally:
        await conn.close()


@pytest.mark.asyncio
async def test_upgrade_to_head(fresh_db):
    _alembic("upgrade", "head")

    assert await _relation_exists("users")
    assert await _relation_exists("password_reset_tokens")
    assert await _relation_exists("games")
    assert await _matview_exists("leaderboard")


@pytest.mark.asyncio
async def test_downgrade_to_base(fresh_db):
    _alembic("upgrade", "head")
    _alembic("downgrade", "base")

    assert not await _relation_exists("users")
    assert not await _relation_exists("password_reset_tokens")
    assert not await _relation_exists("games")
    assert not await _relation_exists("leaderboard")


@pytest.mark.asyncio
async def test_stepwise_upgrade_downgrade(fresh_db):
    # Step 1: users only
    _alembic("upgrade", "0001")
    assert await _relation_exists("users")
    assert not await _relation_exists("password_reset_tokens")

    # Step 2: password_reset_tokens
    _alembic("upgrade", "0002")
    assert await _relation_exists("password_reset_tokens")

    # Step 3: games
    _alembic("upgrade", "0003")
    assert await _relation_exists("games")

    # Step 4: leaderboard (materialized view)
    _alembic("upgrade", "0004")
    assert await _matview_exists("leaderboard")

    # Roll back one step at a time — matview replaced by legacy view
    _alembic("downgrade", "-1")
    assert not await _matview_exists("leaderboard")
    assert await _relation_exists("games")

    _alembic("downgrade", "-1")
    assert not await _relation_exists("games")
    assert await _relation_exists("password_reset_tokens")

    _alembic("downgrade", "-1")
    assert not await _relation_exists("password_reset_tokens")
    assert await _relation_exists("users")

    _alembic("downgrade", "-1")
    assert not await _relation_exists("users")
