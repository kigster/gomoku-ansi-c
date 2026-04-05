"""Create users table

Revision ID: 0001
Revises:
Create Date: 2026-04-04 12:00:00

"""

from collections.abc import Sequence

from alembic import op

revision: str = "0001"
down_revision: str | Sequence[str] | None = None
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.execute("""
        CREATE TABLE users (
            id            UUID        DEFAULT gen_random_uuid() PRIMARY KEY,
            username      TEXT        NOT NULL UNIQUE,
            email         TEXT        UNIQUE,
            password_hash TEXT        NOT NULL,
            games_started BIGINT      NOT NULL DEFAULT 0,
            games_finished BIGINT     NOT NULL DEFAULT 0,
            created_at    TIMESTAMPTZ NOT NULL DEFAULT now(),
            updated_at    TIMESTAMPTZ NOT NULL DEFAULT now()
        )
    """)
    op.execute("CREATE UNIQUE INDEX idx_users_username ON users (lower(username))")
    op.execute(
        "CREATE UNIQUE INDEX idx_users_email ON users (lower(email)) WHERE email IS NOT NULL"
    )


def downgrade() -> None:
    op.execute("DROP INDEX IF EXISTS idx_users_email")
    op.execute("DROP INDEX IF EXISTS idx_users_username")
    op.execute("DROP TABLE IF EXISTS users CASCADE")
