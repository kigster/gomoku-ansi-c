"""Models for the game API."""

from datetime import datetime
from uuid import uuid4

from pydantic import BaseModel


class LeaderboardEntry(BaseModel):
    """A single entry in the leaderboard."""

    id: uuid4()
    ordinal: int
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
    """Response body for the leaderboard."""

    entries: list[LeaderboardEntry]
