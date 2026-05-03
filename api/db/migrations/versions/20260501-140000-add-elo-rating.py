"""Add Elo rating columns to users and games

Revision ID: 0008
Revises: 0007
Create Date: 2026-05-01 14:00:00

Replaces the legacy `games.score` ranking (a function of the difficulty
knobs the human picked when they won) with a proper Elo rating system.
The live formula is classical Elo; the parameters are picked to match
Gomocup's BayesElo philosophy (eloAdvantage=0, eloDraw=0.01) so the
numbers a recalibration job (future work) would compute land in the
same ballpark.

`users` gets:
- `elo_rating`       - current rating (default 1500, K=40 until 20 games)
- `elo_peak`         - highest rating ever reached
- `elo_games_count`  - number of rated games played

`games` gets the per-game audit trail so a future BayesElo recalibration
can reread history:
- `elo_before`           - rated subject's rating BEFORE this game
- `elo_after`            - rated subject's rating AFTER this game
- `opponent_elo_before`  - opponent's rating before the game (AI tier or
                            other human)

The legacy `score` column is preserved for now so the frontend keeps
rendering the old number while the migration is in flight.
"""

from collections.abc import Sequence

from alembic import op

revision: str = "0008"
down_revision: str | Sequence[str] | None = "0007"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.execute(
        """
        ALTER TABLE users
            ADD COLUMN elo_rating INTEGER NOT NULL DEFAULT 1500,
            ADD COLUMN elo_peak INTEGER NOT NULL DEFAULT 1500,
            ADD COLUMN elo_games_count INTEGER NOT NULL DEFAULT 0
        """
    )
    op.execute(
        """
        ALTER TABLE games
            ADD COLUMN elo_before INTEGER,
            ADD COLUMN elo_after INTEGER,
            ADD COLUMN opponent_elo_before INTEGER
        """
    )
    op.execute("CREATE INDEX users_elo_rating_idx ON users (elo_rating DESC, elo_games_count DESC)")


def downgrade() -> None:
    op.execute("DROP INDEX IF EXISTS users_elo_rating_idx")
    op.execute(
        """
        ALTER TABLE games
            DROP COLUMN IF EXISTS elo_before,
            DROP COLUMN IF EXISTS elo_after,
            DROP COLUMN IF EXISTS opponent_elo_before
        """
    )
    op.execute(
        """
        ALTER TABLE users
            DROP COLUMN IF EXISTS elo_rating,
            DROP COLUMN IF EXISTS elo_peak,
            DROP COLUMN IF EXISTS elo_games_count
        """
    )
