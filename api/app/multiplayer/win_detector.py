"""Pure-Python port of `gomoku-c/src/gomoku/gomoku.c:149-188` (`has_winner`).

We use exactly `count == 5` semantics — the standard rule-set per the C
engine. Six-or-more in a row is *not* a win (Renju-ish). When called after a
move at `(x, y)` we only need to walk the four directions through that cell;
the C version scans the whole board for clarity, but the result is identical.
"""

from __future__ import annotations

from collections.abc import Iterable

# Four direction vectors covering all 8 (we walk both ways from the centre).
_DIRS: tuple[tuple[int, int], ...] = ((1, 0), (0, 1), (1, 1), (1, -1))


def has_winner(
    moves: Iterable[tuple[int, int]],
    last_x: int,
    last_y: int,
    last_player: str,
    board_size: int,
) -> bool:
    """Return True if `last_player` just completed a 5-in-a-row at (last_x, last_y).

    `moves` is the full move list **including** the most-recent move at
    (last_x, last_y). Internally we build a sparse {(x,y): player} map of the
    cells belonging to `last_player` only — that's all we need for the count.
    """
    occupied: set[tuple[int, int]] = set()
    parity = 0  # 0 → X (move 0, 2, 4 …), 1 → O
    for mx, my in moves:
        player = "X" if parity == 0 else "O"
        if player == last_player:
            occupied.add((mx, my))
        parity ^= 1

    # Sanity: the supplied last move must belong to last_player.
    if (last_x, last_y) not in occupied:
        return False

    for dx, dy in _DIRS:
        count = 1
        # Walk forward.
        x, y = last_x + dx, last_y + dy
        while 0 <= x < board_size and 0 <= y < board_size and (x, y) in occupied:
            count += 1
            x += dx
            y += dy
        # Walk backward.
        x, y = last_x - dx, last_y - dy
        while 0 <= x < board_size and 0 <= y < board_size and (x, y) in occupied:
            count += 1
            x -= dx
            y -= dy
        if count == 5:
            return True
    return False
