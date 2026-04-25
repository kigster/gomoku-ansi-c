"""Create leaderboard materialized view

Revision ID: 0004
Revises: 0003
Create Date: 2026-04-04 12:00:03

"""

from collections.abc import Sequence

from alembic import op

revision: str = "0004"
down_revision: str | Sequence[str] | None = "0003"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.execute("""
        CREATE MATERIALIZED VIEW leaderboard AS
        SELECT
            u.id                    AS user_id,
            u.username,
            COUNT(*)                AS games_count,
            COUNT(*) FILTER (WHERE g.winner = g.human_player)
                                    AS games_won,
            ROUND(
                100.0 * COUNT(*) FILTER (WHERE g.winner = g.human_player)
                      / GREATEST(COUNT(*), 1),
                1
            )                       AS win_percentage,
            MAX(g.score)            AS max_score,
            ROUND(AVG(g.score))::INT
                                    AS avg_score,
            ROUND(AVG(g.depth)::NUMERIC, 1)
                                    AS ai_depth_average,
            ROUND(AVG(g.radius)::NUMERIC, 1)
                                    AS ai_radius_average,
            ROUND(SUM(g.human_time_s))::BIGINT
                                    AS total_time_played_seconds,
            ROUND(SUM(g.ai_time_s))::BIGINT
                                    AS total_time_ai_seconds,
            ROW_NUMBER() OVER (ORDER BY MAX(g.score) DESC)
                                    AS leaderboard_number
        FROM games g
        JOIN users u ON u.id = g.user_id
        WHERE g.score > 0
        GROUP BY u.id, u.username
        ORDER BY max_score DESC
        WITH NO DATA
    """)

    # Unique index required for REFRESH MATERIALIZED VIEW CONCURRENTLY
    op.execute("CREATE UNIQUE INDEX idx_leaderboard_user ON leaderboard (user_id)")
    op.execute("CREATE INDEX idx_leaderboard_rank ON leaderboard (leaderboard_number)")
    op.execute("CREATE INDEX idx_leaderboard_score ON leaderboard (max_score DESC)")

    op.execute(
        "COMMENT ON MATERIALIZED VIEW leaderboard IS "
        "'Aggregated player stats ranked by max score; "
        "refresh with REFRESH MATERIALIZED VIEW CONCURRENTLY leaderboard'"
    )
    op.execute(
        "COMMENT ON COLUMN leaderboard.user_id IS 'References users(id); one row per player'"
    )
    op.execute("COMMENT ON COLUMN leaderboard.username IS 'Player display name'")
    op.execute(
        "COMMENT ON COLUMN leaderboard.games_count IS "
        "'Total number of completed games with score > 0'"
    )
    op.execute("COMMENT ON COLUMN leaderboard.games_won IS 'Number of games the player won'")
    op.execute(
        "COMMENT ON COLUMN leaderboard.win_percentage IS 'Win rate as a percentage (0.0 to 100.0)'"
    )
    op.execute(
        "COMMENT ON COLUMN leaderboard.max_score IS 'Highest score achieved across all games'"
    )
    op.execute("COMMENT ON COLUMN leaderboard.avg_score IS 'Average score across all scored games'")
    op.execute(
        "COMMENT ON COLUMN leaderboard.ai_depth_average IS "
        "'Average AI search depth across all games'"
    )
    op.execute(
        "COMMENT ON COLUMN leaderboard.ai_radius_average IS "
        "'Average AI search radius across all games'"
    )
    op.execute(
        "COMMENT ON COLUMN leaderboard.total_time_played_seconds IS "
        "'Cumulative human thinking time in seconds'"
    )
    op.execute(
        "COMMENT ON COLUMN leaderboard.total_time_ai_seconds IS "
        "'Cumulative AI computation time in seconds'"
    )
    op.execute(
        "COMMENT ON COLUMN leaderboard.leaderboard_number IS 'Global rank by max_score (1 = best)'"
    )


def downgrade() -> None:
    op.execute("DROP MATERIALIZED VIEW IF EXISTS leaderboard CASCADE")
