"""Add social graph (friendships, blocks) + chat (chats, chat_messages)

Revision ID: 0009
Revises: 0008
Create Date: 2026-05-01 15:00:00

Implements the social-graph and chat schema described in the chat plan
(see frontend/src/components/ChatPanel.tsx for the slash-command
contract). Highlights:

- `friendships` is **unidirectional** — `(user_id, friend_id)` means
  user follows friend. Mutual = both rows present, which is what we
  call "friends". The followee can invite the follower to a game even
  when the follow isn't reciprocal; only the symmetric pair counts as
  friends for any social affordance gated on that.
- `blocks` is also unidirectional — blocker hides blockee. A block
  also implies the friendships in either direction are wiped (handled
  in the router, not the schema, so the constraint stays simple).
- `chats` is a 1:1 mapping to a multiplayer_games row for now; the
  `multiplayer_game_id` FK becomes nullable in a future migration when
  we add stand-alone DM between mutual friends.
- `chat_messages` is append-only, capped to 2000 chars per row.
"""

from collections.abc import Sequence

from alembic import op

revision: str = "0009"
down_revision: str | Sequence[str] | None = "0008"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.execute(
        """
        CREATE TABLE chats (
            id                  UUID PRIMARY KEY DEFAULT gen_random_uuid(),
            multiplayer_game_id UUID NOT NULL UNIQUE
                                REFERENCES multiplayer_games(id) ON DELETE CASCADE,
            created_at          TIMESTAMPTZ NOT NULL DEFAULT now()
        )
        """
    )
    op.execute(
        """
        CREATE TABLE chat_messages (
            id          UUID PRIMARY KEY DEFAULT gen_random_uuid(),
            chat_id     UUID NOT NULL REFERENCES chats(id) ON DELETE CASCADE,
            speaker_id  UUID NOT NULL REFERENCES users(id),
            message     TEXT NOT NULL CHECK (length(message) BETWEEN 1 AND 2000),
            created_at  TIMESTAMPTZ NOT NULL DEFAULT now()
        )
        """
    )
    op.execute(
        "CREATE INDEX chat_messages_chat_created_idx "
        "ON chat_messages (chat_id, created_at)"
    )

    op.execute(
        """
        CREATE TABLE friendships (
            id          UUID PRIMARY KEY DEFAULT gen_random_uuid(),
            user_id     UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            friend_id   UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
            UNIQUE (user_id, friend_id),
            CHECK (user_id <> friend_id)
        )
        """
    )
    op.execute("CREATE INDEX friendships_friend_id_idx ON friendships (friend_id)")

    op.execute(
        """
        CREATE TABLE blocks (
            blocker_id  UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            blocked_id  UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
            PRIMARY KEY (blocker_id, blocked_id),
            CHECK (blocker_id <> blocked_id)
        )
        """
    )
    op.execute("CREATE INDEX blocks_blocked_id_idx ON blocks (blocked_id)")


def downgrade() -> None:
    op.execute("DROP TABLE IF EXISTS blocks")
    op.execute("DROP TABLE IF EXISTS friendships")
    op.execute("DROP TABLE IF EXISTS chat_messages")
    op.execute("DROP TABLE IF EXISTS chats")
