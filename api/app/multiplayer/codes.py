"""Game-code generation for multiplayer rooms.

Plan §6: 6-character Crockford-base32 (without I, L, O, U, 0, 1) gives a
~729M codespace and remains readable when shared verbally.
"""

from __future__ import annotations

import secrets

ALPHABET = "23456789ABCDEFGHJKMNPQRSTVWXYZ"  # 30 chars
CODE_LEN = 6


def new_code() -> str:
    """Return a fresh 6-char base32 code."""
    return "".join(secrets.choice(ALPHABET) for _ in range(CODE_LEN))
