"""Create multiplayer_games table for human-vs-human play

Revision ID: 0006
Revises: 0005
Create Date: 2026-05-01 12:01:00

"""

from collections.abc import Sequence

from alembic import op

revision: str = "0006"
down_revision: str | Sequence[str] | None = "0005"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    # Defensive: if a previous upgrade was rolled back by truncating
    # alembic_version directly (e.g. the test_migrations.fresh_db fixture),
    # the table can linger. In normal use this is a no-op.
    op.execute("DROP TABLE IF EXISTS multiplayer_games CASCADE")
    op.execute("""
        CREATE TABLE multiplayer_games (
            id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
            code            VARCHAR(8)  NOT NULL UNIQUE,
            host_user_id    UUID        NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            guest_user_id   UUID        REFERENCES users(id) ON DELETE SET NULL,
            host_color      CHAR(1)     NOT NULL CHECK (host_color IN ('X','O')),
            board_size      INTEGER     NOT NULL DEFAULT 15 CHECK (board_size IN (15, 19)),
            rule_set        VARCHAR(16) NOT NULL DEFAULT 'freestyle',
            state           VARCHAR(16) NOT NULL DEFAULT 'waiting'
                            CHECK (state IN ('waiting','in_progress','finished','abandoned')),
            winner          CHAR(1)     CHECK (winner IS NULL OR winner IN ('X','O','draw')),
            moves           JSONB       NOT NULL DEFAULT '[]'::JSONB,
            next_to_move    CHAR(1)     NOT NULL DEFAULT 'X',
            version         INTEGER     NOT NULL DEFAULT 0,
            created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
            updated_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
            finished_at     TIMESTAMPTZ
        )
    """)
    op.execute("CREATE INDEX multiplayer_games_code_idx     ON multiplayer_games (code)")
    op.execute(
        "CREATE INDEX multiplayer_games_host_idx     "
        "ON multiplayer_games (host_user_id, created_at DESC)"
    )
    op.execute(
        "CREATE INDEX multiplayer_games_guest_idx    "
        "ON multiplayer_games (guest_user_id, created_at DESC)"
    )
    op.execute(
        "CREATE INDEX multiplayer_games_active_idx   "
        "ON multiplayer_games (state) WHERE state IN ('waiting','in_progress')"
    )
    op.execute(
        "CREATE INDEX multiplayer_games_updated_idx  "
        "ON multiplayer_games (updated_at DESC)"
    )

    # Multiplayer human-vs-human games end up with depth=0/radius=0/total_moves>=0,
    # which would violate the original CHECK constraints on the `games` table
    # (created in 0003). Relax them to also accept 0, which signals
    # "human opponent, not AI" per `doc/human-vs-human-plan.md` §3.
    op.execute("ALTER TABLE games DROP CONSTRAINT IF EXISTS games_depth_check")
    op.execute("ALTER TABLE games DROP CONSTRAINT IF EXISTS games_radius_check")
    op.execute("ALTER TABLE games DROP CONSTRAINT IF EXISTS games_total_moves_check")
    op.execute(
        "ALTER TABLE games ADD CONSTRAINT games_depth_check "
        "CHECK (depth BETWEEN 0 AND 10)"
    )
    op.execute(
        "ALTER TABLE games ADD CONSTRAINT games_radius_check "
        "CHECK (radius BETWEEN 0 AND 5)"
    )
    op.execute(
        "ALTER TABLE games ADD CONSTRAINT games_total_moves_check "
        "CHECK (total_moves >= 0)"
    )


def downgrade() -> None:
    op.execute("ALTER TABLE games DROP CONSTRAINT IF EXISTS games_total_moves_check")
    op.execute("ALTER TABLE games DROP CONSTRAINT IF EXISTS games_radius_check")
    op.execute("ALTER TABLE games DROP CONSTRAINT IF EXISTS games_depth_check")
    op.execute(
        "ALTER TABLE games ADD CONSTRAINT games_depth_check "
        "CHECK (depth BETWEEN 1 AND 10)"
    )
    op.execute(
        "ALTER TABLE games ADD CONSTRAINT games_radius_check "
        "CHECK (radius BETWEEN 1 AND 5)"
    )
    op.execute(
        "ALTER TABLE games ADD CONSTRAINT games_total_moves_check "
        "CHECK (total_moves > 0)"
    )
    op.execute("DROP INDEX IF EXISTS multiplayer_games_updated_idx")
    op.execute("DROP INDEX IF EXISTS multiplayer_games_active_idx")
    op.execute("DROP INDEX IF EXISTS multiplayer_games_guest_idx")
    op.execute("DROP INDEX IF EXISTS multiplayer_games_host_idx")
    op.execute("DROP INDEX IF EXISTS multiplayer_games_code_idx")
    op.execute("DROP TABLE IF EXISTS multiplayer_games CASCADE")
