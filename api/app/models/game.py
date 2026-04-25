"""Models for the game API."""

from datetime import datetime

from pydantic import BaseModel


class GameSaveRequest(BaseModel):
    """Request body for saving a completed game."""

    game_json: dict


class GameSaveResponse(BaseModel):
    """Response body for saving a completed game."""

    id: str
    score: int
    rating: float


class GameHistoryEntry(BaseModel):
    """A single game entry in the user's game history."""

    id: str
    username: str
    won: bool
    score: int
    depth: int
    human_time_s: float
    ai_time_s: float
    played_at: datetime


class GameHistoryResponse(BaseModel):
    """Response body for the user's game history."""

    games: list[GameHistoryEntry]
