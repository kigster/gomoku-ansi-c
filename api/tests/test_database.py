"""Tests for database pool lifecycle edge cases."""

import pytest

import app.database as db_mod


@pytest.mark.asyncio
async def test_get_pool_raises_when_not_initialized():
    original = db_mod.pool
    try:
        db_mod.pool = None
        with pytest.raises(RuntimeError, match="not initialized"):
            await db_mod.get_pool()
    finally:
        db_mod.pool = original


@pytest.mark.asyncio
async def test_close_pool_when_already_none():
    original = db_mod.pool
    try:
        db_mod.pool = None
        await db_mod.close_pool()
        assert db_mod.pool is None
    finally:
        db_mod.pool = original
