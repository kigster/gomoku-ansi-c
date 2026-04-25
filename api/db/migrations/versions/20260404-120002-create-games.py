"""Create games table, scoring functions, and pending-geo view

Revision ID: 0003
Revises: 0002
Create Date: 2026-04-04 12:00:02

"""

from collections.abc import Sequence

from alembic import op

revision: str = "0003"
down_revision: str | Sequence[str] | None = "0002"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    # Games table
    op.execute("""
        CREATE TABLE games (
            id            UUID        DEFAULT gen_random_uuid() PRIMARY KEY,
            username      TEXT        NOT NULL,
            user_id       UUID        REFERENCES users(id),
            winner        TEXT        NOT NULL CHECK (winner IN ('X', 'O', 'draw')),
            human_player  TEXT        NOT NULL CHECK (human_player IN ('X', 'O')),
            board_size    INT         NOT NULL CHECK (board_size IN (15, 19)),
            depth         INT         NOT NULL CHECK (depth BETWEEN 1 AND 10),
            radius        INT         NOT NULL CHECK (radius BETWEEN 1 AND 5),
            total_moves   INT         NOT NULL CHECK (total_moves > 0),
            human_time_s  DOUBLE PRECISION NOT NULL,
            ai_time_s     DOUBLE PRECISION NOT NULL,
            score         INT         NOT NULL DEFAULT 0,
            game_json     JSONB       NOT NULL,
            client_ip     INET,
            geo_country   TEXT,
            geo_region    TEXT,
            geo_city      TEXT,
            geo_loc       POINT,
            played_at     TIMESTAMPTZ NOT NULL DEFAULT now()
        )
    """)

    # Indexes
    op.execute("CREATE INDEX idx_games_player ON games (username)")
    op.execute("CREATE INDEX idx_games_played ON games (played_at DESC)")
    op.execute("CREATE INDEX idx_games_score  ON games (score DESC)")
    op.execute("CREATE INDEX idx_games_ip     ON games (client_ip)")
    op.execute("CREATE INDEX idx_games_user   ON games (user_id)")

    # View: pending geolocation
    op.execute("""
        CREATE OR REPLACE VIEW games_pending_geo AS
        SELECT id, client_ip
        FROM games
        WHERE client_ip IS NOT NULL
          AND geo_country IS NULL
    """)

    # Function: time bonus/penalty
    op.execute("""
        CREATE OR REPLACE FUNCTION game_time_score(seconds DOUBLE PRECISION)
        RETURNS DOUBLE PRECISION
        LANGUAGE sql IMMUTABLE PARALLEL SAFE AS $$
            SELECT CASE
                WHEN seconds <= 120 THEN 99.77 * (exp(-0.02 * (seconds - 120)) - 1)
                ELSE -28.11 * ln(1.0 + 0.071 * (seconds - 120))
            END
        $$
    """)

    # Function: full score calculation
    op.execute("""
        CREATE OR REPLACE FUNCTION game_score(
            human_won BOOLEAN,
            depth     INT,
            radius    INT,
            human_seconds DOUBLE PRECISION
        ) RETURNS INT
        LANGUAGE sql IMMUTABLE PARALLEL SAFE AS $$
            SELECT CASE
                WHEN NOT human_won THEN 0
                ELSE GREATEST(0, (1000 * depth + 50 * radius + game_time_score(human_seconds))::INT)
            END
        $$
    """)


def downgrade() -> None:
    op.execute("DROP FUNCTION IF EXISTS game_score(BOOLEAN, INT, INT, DOUBLE PRECISION)")
    op.execute("DROP FUNCTION IF EXISTS game_time_score(DOUBLE PRECISION)")
    op.execute("DROP VIEW IF EXISTS games_pending_geo")
    op.execute("DROP TABLE IF EXISTS games CASCADE")
