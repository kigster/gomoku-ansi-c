"""Models for the leaderboard API."""

from datetime import datetime

from pydantic import BaseModel


class LeaderboardEntry(BaseModel):
    username: str
    score: int
    rating: float
    depth: int
    radius: int
    total_moves: int
    human_time_s: float
    geo_country: str | None = None
    geo_city: str | None = None
    played_at: datetime


class LeaderboardResponse(BaseModel):
    entries: list[LeaderboardEntry]
