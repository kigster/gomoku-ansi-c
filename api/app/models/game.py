from datetime import datetime

from pydantic import BaseModel


class GameSaveRequest(BaseModel):
    game_json: dict


class GameSaveResponse(BaseModel):
    id: str
    score: int
    rating: float


class GameHistoryEntry(BaseModel):
    id: str
    player_name: str
    won: bool
    score: int
    depth: int
    human_time_s: float
    ai_time_s: float
    played_at: datetime


class GameHistoryResponse(BaseModel):
    games: list[GameHistoryEntry]
