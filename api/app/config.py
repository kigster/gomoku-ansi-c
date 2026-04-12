"""Settings for the application."""

from typing import Literal

from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    """Settings for the application."""

    environment: Literal["development", "production", "test", "ci"] = "development"

    # Public Domain
    public_domain: str = "app.gomoku.games"

    # Database
    db_socket: str = ""
    db_name: str = "gomoku"
    db_user: str = "postgres"
    db_password: str = ""
    database_url: str = ""

    # Upstream game engine
    gomoku_httpd_url: str = "http://localhost:10000"

    # JWT
    jwt_secret: str = "change-me-in-production"
    jwt_algorithm: str = "HS256"
    jwt_expire_minutes: int = 7 * 1440 # 1 week

    # CORS
    cors_origins: list[str] = ["*"]

    # Email
    email_provider: str = "stdout"  # stdout | sendgrid
    email_from: str = "noreply@gomoku.games"
    sendgrid_api_key: str = ""

    # LogFire
    logfire_token: str | None = None

    # LLMs
    openai_api_token: str | None = None
    anthropic_api_token: str | None = None

    @property
    def database_dsn(self) -> str:
        if self.database_url:
            return self.database_url
        password_part = f":{self.db_password}" if self.db_password else ""
        if self.db_socket:
            return (
                f"postgresql://{self.db_user}{password_part}@/{self.db_name}?host={self.db_socket}"
            )
        return f"postgresql://{self.db_user}{password_part}@localhost/{self.db_name}"

    model_config = {"env_prefix": "", "env_file": ".env", "extra": "ignore"}


settings = Settings()
