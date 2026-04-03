from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    # Database
    db_socket: str = ""
    db_name: str = "gomoku"
    db_user: str = "postgres"
    db_password: str = ""
    database_url: str = ""

    # Upstream game engine
    gomoku_httpd_url: str = "http://localhost:8787"

    # JWT
    jwt_secret: str = "change-me-in-production"
    jwt_algorithm: str = "HS256"
    jwt_expire_minutes: int = 1440  # 24 hours

    # CORS
    cors_origins: list[str] = ["*"]

    # Email
    email_provider: str = "stdout"  # stdout | sendgrid
    email_from: str = "noreply@gomoku.games"
    sendgrid_api_key: str = ""

    @property
    def database_dsn(self) -> str:
        if self.database_url:
            return self.database_url
        password_part = f":{self.db_password}" if self.db_password else ""
        if self.db_socket:
            return (
                f"postgresql://{self.db_user}{password_part}"
                f"@/{self.db_name}?host={self.db_socket}"
            )
        return f"postgresql://{self.db_user}{password_part}@localhost/{self.db_name}"

    model_config = {"env_prefix": "", "env_file": ".env", "extra": "ignore"}


settings = Settings()
