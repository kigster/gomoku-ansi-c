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
    rows = await pool.fetch(
        """SELECT username, score, depth, radius, total_moves,
                  round(human_time_s::numeric, 1) AS human_time_s,
                  geo_country, geo_city, played_at
           FROM games
           WHERE score > 0
           ORDER BY score DESC
           LIMIT $1""",
        limit,
    )
    entries = [
        LeaderboardEntry(
            username=r["username"],
            score=r["score"],
            rating=rating(r["score"]),
            depth=r["depth"],
            radius=r["radius"],
            total_moves=r["total_moves"],
            human_time_s=float(r["human_time_s"]),
            geo_country=r["geo_country"],
            geo_city=r["geo_city"],
            played_at=r["played_at"],
        )
        for r in rows
    ]
    return LeaderboardResponse(entries=entries)
