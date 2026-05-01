"""Routes for the leaderboard API."""

from fastapi import APIRouter, Depends, Query

from app.database import get_pool
from app.models.types import LeaderboardEntry, LeaderboardResponse
from app.scoring import rating

router = APIRouter(prefix="/leaderboard", tags=["leaderboard"])


@router.get("", response_model=LeaderboardResponse)
async def get_leaderboard(
    limit: int = Query(default=100, ge=1, le=500),
    pool=Depends(get_pool),
):
    """Get the global leaderboard, ranked by Elo.

    One row per registered user. Players are ordered by `elo_rating`
    (descending) as long as they've played at least one rated game. The
    legacy ``score``/``depth``/``radius``/``human_time_s`` fields show
    the user's best AI win so the existing frontend keeps a meaningful
    secondary sort while the UI transitions to displaying Elo
    prominently.
    """
    rows = await pool.fetch(
        """
        WITH best_ai AS (
            SELECT DISTINCT ON (user_id)
                   user_id, score, depth, radius, total_moves,
                   human_time_s, geo_country, geo_city, played_at
            FROM games
            WHERE user_id IS NOT NULL
              AND game_type = 'ai'
              AND score > 0
            ORDER BY user_id, score DESC, played_at ASC
        )
        SELECT u.username,
               u.elo_rating,
               u.elo_games_count,
               COALESCE(b.score, 0)            AS score,
               COALESCE(b.depth, 0)            AS depth,
               COALESCE(b.radius, 0)           AS radius,
               COALESCE(b.total_moves, 0)      AS total_moves,
               COALESCE(round(b.human_time_s::numeric, 1), 0) AS human_time_s,
               b.geo_country,
               b.geo_city,
               COALESCE(b.played_at, u.created_at) AS played_at
        FROM users u
        LEFT JOIN best_ai b ON b.user_id = u.id
        WHERE u.elo_games_count >= 1
          -- Match the legacy semantic: a player only appears once they
          -- have at least one winning AI game on record.
          AND b.user_id IS NOT NULL
        ORDER BY u.elo_rating DESC, u.elo_games_count DESC, u.created_at ASC
        LIMIT $1
        """,
        limit,
    )
    entries = [
        LeaderboardEntry(
            username=r["username"],
            elo_rating=int(r["elo_rating"]),
            elo_games_count=int(r["elo_games_count"]),
            score=int(r["score"]),
            rating=rating(int(r["score"])),
            depth=int(r["depth"]),
            radius=int(r["radius"]),
            total_moves=int(r["total_moves"]),
            human_time_s=float(r["human_time_s"]),
            geo_country=r["geo_country"],
            geo_city=r["geo_city"],
            played_at=r["played_at"],
        )
        for r in rows
    ]
    return LeaderboardResponse(entries=entries)
