from pydantic import BaseModel


class GameSaveRequest(BaseModel):
    game_json: dict


class GameSaveResponse(BaseModel):
    id: str
    score: int
    rating: float
