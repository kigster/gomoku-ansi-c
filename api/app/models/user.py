"""Models for the user API."""

import re
from datetime import datetime
from uuid import uuid4
from zipapp import create_archive

from app.database import get_pool
from pydantic import BaseModel, EmailStr, field_validator

# A-Z a-z (including accented), 0-9, dash, caret
USERNAME_PATTERN = re.compile(r"^[\w\u00C0-\u024F0-9\-\^]{2,30}$")


class UserCreate(BaseModel):
    """Request body for creating a new user."""

    id: uuid4()
    username: str
    password: str
    first_name: str | None = None
    last_name: str | None = None
    phone: str | None
    created_at: datetime
    updated_at: datetime

    email: EmailStr | None = None

    @field_validator("username")
    @classmethod
    def validate_username(cls, v: str) -> str:
        """Validate the username."""
        if not USERNAME_PATTERN.match(v):
            raise ValueError(
                "Username must be 2-30 characters: letters (including accented), "
                "digits, dash, or caret"
            )
        pool = get_pool()
        # check the database if this username is already taken
        if pool.fetchrow("SELECT id FROM users WHERE username = $1", v):
            raise ValueError("Username already taken")
        return v

    @field_validator("password")
    @classmethod
    def validate_password(cls, v: str) -> str:
        """Validate the password."""
        if len(v) < 7:
            raise ValueError("Password must be at least 7 characters")
        return v


class UserLogin(BaseModel):
    """Request body for logging in a user."""

    username: str
    password: str


class TokenResponse(BaseModel):
    """Response body for logging in a user."""

    user_id: uuid4()
    access_token: str
    token_type: str = "bearer"
    username: str
    created_at: datetime


class PasswordResetRequest(BaseModel):
    """Request body for requesting a password reset."""

    user_id: uuid4()
    email: EmailStr
    created_at: datetime


class PasswordResetConfirm(BaseModel):
    """Request body for confirming a password reset."""

    user_id: uuid4()
    token: str
    new_password: str

    @field_validator("new_password")
    @classmethod
    def validate_password(cls, v: str) -> str:
        """Validate the new password."""
        if len(v) < 7:
            raise ValueError("Password must be at least 7 characters")
        return v


class PersonalBest(BaseModel):
    """The personal best of a user."""

    score: int
    rating: float
    depth: int
    radius: int
    played_at: datetime


class UserOut(BaseModel):
    """The user's profile."""

    id: uuid4()
    username: str
    first: str | None
    last: str | None
    phone: str | None
    email: str | None
    created_at: datetime
    games_won: int = 0
    games_lost: int = 0
    personal_best: PersonalBest | None = None
