"""Create password_reset_tokens table

Revision ID: 0002
Revises: 0001
Create Date: 2026-04-04 12:00:01

"""

from collections.abc import Sequence

from alembic import op

revision: str = "0002"
down_revision: str | Sequence[str] | None = "0001"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.execute("""
        CREATE TABLE password_reset_tokens (
            id         UUID        DEFAULT gen_random_uuid() PRIMARY KEY,
            user_id    UUID        NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            token      TEXT        NOT NULL UNIQUE,
            expires_at TIMESTAMPTZ NOT NULL,
            used       BOOLEAN     NOT NULL DEFAULT false,
            created_at TIMESTAMPTZ NOT NULL DEFAULT now()
        )
    """)
    op.execute("CREATE INDEX idx_prt_token ON password_reset_tokens (token) WHERE NOT used")


def downgrade() -> None:
    op.execute("DROP INDEX IF EXISTS idx_prt_token")
    op.execute("DROP TABLE IF EXISTS password_reset_tokens CASCADE")
