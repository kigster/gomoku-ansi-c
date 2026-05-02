"""Add multiplayer_games.created_via for invite rate-limit accounting

Revision ID: 0010
Revises: 0009
Create Date: 2026-05-01 16:00:00

The /chat/invite rate limit (7/hour, 15/24h per caller) must count
invite-created rows only — not the modal's POST /multiplayer/new
rooms. Without a discriminator, opening the multiplayer modal a few
times eats invite quota the user can't see, and chat-invites can be
bypassed by alternating with modal creates.

Default 'modal' for any pre-existing rows: at the time this migration
lands, no production row was created via the chat-invite path (the
endpoint is shipping in the same release), so backfilling them as
modal-created is correct.

The CHECK keeps the column to a closed enum without a separate type
(closed-enum-via-CHECK is the project convention, see e.g.
multiplayer_games.state).
"""

from collections.abc import Sequence

from alembic import op

revision: str = "0010"
down_revision: str | Sequence[str] | None = "0009"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.execute(
        """
        ALTER TABLE multiplayer_games
            ADD COLUMN created_via TEXT NOT NULL DEFAULT 'modal'
                CHECK (created_via IN ('modal', 'invite'))
        """
    )
    # Partial index supporting the per-caller rolling-window count in
    # /chat/invite. Filtering on `created_via='invite'` keeps the index
    # small (modal rows dominate) and lets the rate-limit query stay a
    # cheap range scan: WHERE host_user_id=? AND created_via='invite'
    # AND created_at > now()-interval '24h'.
    op.execute(
        """
        CREATE INDEX multiplayer_games_invite_host_created_idx
        ON multiplayer_games (host_user_id, created_at DESC)
        WHERE created_via = 'invite'
        """
    )


def downgrade() -> None:
    op.execute("DROP INDEX IF EXISTS multiplayer_games_invite_host_created_idx")
    op.execute("ALTER TABLE multiplayer_games DROP COLUMN IF EXISTS created_via")
