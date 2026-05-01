"""Add opponent_id to games for human-vs-human bookkeeping

Revision ID: 0007
Revises: 0006
Create Date: 2026-05-01 13:00:00

A multiplayer game writes two `games` rows (one per participant) — see
`api/app/routers/multiplayer.py::_write_finished_games_rows`. Connecting
those two rows to each other previously required digging through the
`game_json` JSONB blob. This migration adds a first-class
`opponent_id UUID REFERENCES users(id)` column on `games`:

- AI rows: `opponent_id IS NULL` (enforced by the conditional CHECK
  introduced alongside `game_type` in 0006).
- Multiplayer rows: `opponent_id` is the OTHER participant's user_id.

The FK uses `ON DELETE SET NULL` so deleting a user doesn't cascade-
delete their opponent's history. Plain index for opponent lookups.
"""

from collections.abc import Sequence

from alembic import op

revision: str = "0007"
down_revision: str | Sequence[str] | None = "0006"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.execute("""
        ALTER TABLE games
            ADD COLUMN opponent_id UUID REFERENCES users(id) ON DELETE SET NULL
    """)
    op.execute(
        "CREATE INDEX games_opponent_id_idx ON games (opponent_id) "
        "WHERE opponent_id IS NOT NULL"
    )
    # AI rows must always have opponent_id NULL; multiplayer rows must have
    # it populated. No backfill needed: at install time, no multiplayer rows
    # exist on production (the multiplayer feature hasn't shipped yet).
    op.execute(
        "ALTER TABLE games ADD CONSTRAINT games_opponent_id_consistency "
        "CHECK ("
        " (game_type = 'ai' AND opponent_id IS NULL) OR "
        " (game_type = 'multiplayer' AND opponent_id IS NOT NULL)"
        ")"
    )


def downgrade() -> None:
    op.execute("ALTER TABLE games DROP CONSTRAINT IF EXISTS games_opponent_id_consistency")
    op.execute("DROP INDEX IF EXISTS games_opponent_id_idx")
    op.execute("ALTER TABLE games DROP COLUMN IF EXISTS opponent_id")
