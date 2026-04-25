"""Add first_name, last_name, last_logged_in_at, logins_count to users

Revision ID: 0005
Revises: 0004
Create Date: 2026-04-05 12:00:00

"""

from collections.abc import Sequence

from alembic import op

revision: str = "0005"
down_revision: str | Sequence[str] | None = "0004"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.execute("""
        ALTER TABLE users
            ADD COLUMN first_name      TEXT,
            ADD COLUMN last_name       TEXT,
            ADD COLUMN last_logged_in_at TIMESTAMPTZ,
            ADD COLUMN logins_count    BIGINT NOT NULL DEFAULT 0
    """)


def downgrade() -> None:
    op.execute("""
        ALTER TABLE users
            DROP COLUMN IF EXISTS first_name,
            DROP COLUMN IF EXISTS last_name,
            DROP COLUMN IF EXISTS last_logged_in_at,
            DROP COLUMN IF EXISTS logins_count
    """)
