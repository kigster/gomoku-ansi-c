"""Helpers for the human-vs-human multiplayer feature.

See `doc/human-vs-human-plan.md` for the design. Keep this package
side-effect free so importing it doesn't require a DB pool.
"""

from app.multiplayer.codes import ALPHABET, new_code
from app.multiplayer.win_detector import has_winner

__all__ = ["ALPHABET", "new_code", "has_winner"]
