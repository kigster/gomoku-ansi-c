"""Models for the game API."""

from datetime import datetime

from pydantic import BaseModel


class GameSaveRequest(BaseModel):
    """Request body for saving a completed game."""

    game_json: dict


class GameSaveResponse(BaseModel):
    """Response body for saving a completed game.

    The legacy ``score`` / ``rating`` fields are preserved while the
    frontend transitions to the Elo display. ``elo_*`` fields are the
    canonical post-game numbers.
    """

    id: str
    score: int
    rating: float
    elo_before: int | None = None
    elo_after: int | None = None
    elo_delta: int | None = None


class GameHistoryEntry(BaseModel):
    """A single game entry in the user's game history.

    `opponent_username` is "AI" for AI games and the other participant's
    username for multiplayer games. `game_type` lets the frontend hide
    AI-specific columns (depth, score) for multiplayer rows.
    """

    id: str
    username: str
    won: bool
    score: int
    depth: int
    human_time_s: float
    ai_time_s: float
    played_at: datetime
    game_type: str  # 'ai' | 'multiplayer'
    opponent_username: str
    elo_before: int | None = None
    elo_after: int | None = None
    opponent_elo_before: int | None = None


class GameHistoryResponse(BaseModel):
    """Response body for the user's game history."""

    games: list[GameHistoryEntry]
