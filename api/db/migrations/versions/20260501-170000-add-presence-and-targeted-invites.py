"""Add presence (users.last_seen_at) + targeted invites (intended_guest_id)

Revision ID: 0011
Revises: 0010
Create Date: 2026-05-01 17:00:00

Two related additions, both required to make /chat/invite functional
end-to-end (invitee actually receives the invite) and to power /who:

1. `users.last_seen_at` — presence timestamp. Bumped on every
   authenticated request via `get_current_user`. The /social/who
   endpoint surfaces users with last_seen_at within a recent window
   so the chat panel can list "who's around to invite."

2. `multiplayer_games.intended_guest_id` — targets a specific user
   for an invite-created game (NULL for modal-created rooms, where
   the host hands out the URL ad-hoc). The new GET /chat/incoming
   uses this to deliver pending invites to the intended recipient
   on a polling tier.

Both columns back real features that depend on them; the design does
not work without these rows existing.
"""

from collections.abc import Sequence

from alembic import op

revision: str = "0011"
down_revision: str | Sequence[str] | None = "0010"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    # Presence: every existing user is treated as "just seen" at
    # migration time so /who doesn't show every account as offline
    # until they next log in. Subsequent activity replaces this.
    op.execute(
        """
        ALTER TABLE users
            ADD COLUMN last_seen_at TIMESTAMPTZ NOT NULL DEFAULT now()
        """
    )
    # Single-column DESC index supports /social/who's
    # `WHERE last_seen_at > NOW() - INTERVAL '60s' ORDER BY ... DESC`.
    op.execute("CREATE INDEX users_last_seen_idx ON users (last_seen_at DESC)")

    # Targeted invites. Nullable because /multiplayer/new (modal flow)
    # never has an intended guest at creation time. /chat/invite always
    # sets it. ON DELETE SET NULL: if the target deletes their account,
    # the invite stops being deliverable but the game row survives.
    op.execute(
        """
        ALTER TABLE multiplayer_games
            ADD COLUMN intended_guest_id UUID
                REFERENCES users(id) ON DELETE SET NULL
        """
    )
    # Partial index supporting GET /chat/incoming. The query is
    # WHERE intended_guest_id = ? AND state = 'waiting'
    # AND expires_at > NOW(). Filtering on the active state in the
    # WHERE keeps the index small (most rows are eventually
    # cancelled/finished/abandoned).
    op.execute(
        """
        CREATE INDEX multiplayer_games_intended_guest_active_idx
        ON multiplayer_games (intended_guest_id, created_at DESC)
        WHERE state = 'waiting' AND intended_guest_id IS NOT NULL
        """
    )


def downgrade() -> None:
    op.execute("DROP INDEX IF EXISTS multiplayer_games_intended_guest_active_idx")
    op.execute("ALTER TABLE multiplayer_games DROP COLUMN IF EXISTS intended_guest_id")
    op.execute("DROP INDEX IF EXISTS users_last_seen_idx")
    op.execute("ALTER TABLE users DROP COLUMN IF EXISTS last_seen_at")
