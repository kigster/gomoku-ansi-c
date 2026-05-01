"""Pydantic request/response schemas for the multiplayer router.

These are not SQLAlchemy models (the codebase is asyncpg-only) — purely
wire shapes. `MultiplayerGameView` is the full participant view;
`MultiplayerGamePreview` is the slim view returned to non-participants.
"""

from __future__ import annotations

from datetime import datetime
from typing import Literal

from pydantic import BaseModel, Field, conint

# --- Request bodies ---------------------------------------------------------


class NewMultiplayerGameRequest(BaseModel):
    """Body of POST /multiplayer/new. All fields optional."""

    board_size: Literal[15, 19] = 15
    host_color: Literal["X", "O"] = "X"


class JoinRequest(BaseModel):
    """Body of POST /multiplayer/{code}/join. Empty placeholder — auth identifies the joiner."""

    pass


class MoveRequest(BaseModel):
    """Body of POST /multiplayer/{code}/move."""

    x: conint(ge=0, le=18)  # type: ignore[valid-type]
    y: conint(ge=0, le=18)  # type: ignore[valid-type]
    expected_version: int = Field(ge=0)


class ResignRequest(BaseModel):
    pass


# --- Response shapes --------------------------------------------------------


class PlayerInfo(BaseModel):
    """Public-facing identity for a participant."""

    username: str
    color: Literal["X", "O"]


class MultiplayerGameView(BaseModel):
    """Full view returned to participants (host or guest)."""

    code: str
    state: Literal["waiting", "in_progress", "finished", "abandoned"]
    board_size: int
    rule_set: str
    host: PlayerInfo
    guest: PlayerInfo | None
    moves: list[tuple[int, int]]
    next_to_move: Literal["X", "O"]
    winner: Literal["X", "O", "draw"] | None
    your_color: Literal["X", "O"] | None
    your_turn: bool
    version: int
    created_at: datetime
    finished_at: datetime | None


class MultiplayerGamePreview(BaseModel):
    """Slim preview for non-participants. NB: no `moves` key."""

    code: str
    state: Literal["waiting", "in_progress", "finished", "abandoned"]
    board_size: int
    rule_set: str
    host: PlayerInfo
    guest: PlayerInfo | None
    next_to_move: Literal["X", "O"]
    winner: Literal["X", "O", "draw"] | None
    your_color: None = None
    your_turn: bool = False
    version: int
    created_at: datetime
    finished_at: datetime | None
