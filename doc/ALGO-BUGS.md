# Algorithm Bugs

## BUG-001: Closed Four Misclassified as Immediate Win (Critical)

**Status**: Fixed
**Severity**: Critical — caused the AI to lose games it should win or draw
**File**: `src/gomoku/ai.c`, function `find_best_ai_move()`
**Affected Lines**: ~1090 (Step 1 threshold), ~162 (move priority ordering), ~545 (VCT)

### Summary

The AI's top-level move selection (Step 1 in `find_best_ai_move`) treats a
**closed four** (threat score 100,000) as an "immediate winning move" and
returns it without checking whether the opponent has a more urgent threat.
A closed four is blockable — the opponent can play at the single open end.
Only an open four (500,000) or an actual five-in-a-row (1,000,000) are
truly unblockable.

### Root Cause

In `find_best_ai_move()`, Step 1 scans all candidate moves for the AI's own
threat and short-circuits if any move scores >= 100,000:

```c
// STEP 1: Check for immediate winning moves — line ~1090
if (threat >= 100000) {
    winning_moves_x[winning_move_count] = moves[i].x;
    winning_moves_y[winning_move_count] = moves[i].y;
    winning_move_count++;
}
// ...
if (winning_move_count > 0) {
    // Pick one at random and RETURN immediately
    *best_x = winning_moves_x[selected];
    *best_y = winning_moves_y[selected];
    return;  // <-- Step 2 (blocking) is NEVER reached
}
```

The threshold `100,000` includes **closed fours** (4 in a row with one end
blocked). A closed four is NOT an immediate win because:

1. The opponent gets to move before the AI can extend to five.
2. The opponent can block the single open end.
3. Meanwhile, the opponent may have their own winning threat that is
   even more urgent.

### Reproduction: Game 2026-03-02T09-04-05

Recorded in `gomoku-game-2026-03-02T09-04-05.json`. The critical moment is
at move index 31 (O's turn, the AI playing as O with depth=4, radius=2).

#### Board State Before Move 31

Using 0-based coordinates (as in the JSON API notation):

```
Row 4:  . . . . . . . . . . . X X O O O . . .
        A B C D E F G H J K L M N O P Q R S T
```

O has three-in-a-row at cols 13-15 in row 4. Col 12 = X (blocked left).
Col 16 (R) is empty and open.

```
Row 6:  . . . . . . . . . X X O X X X X . . .
        A B C D E F G H J K L M N O P Q R S T
```

X has four-in-a-row at cols 12-15 in row 6. Col 11 = O (blocked left).
Col 16 (R) is empty and open.

#### What Happened

1. Step 1 evaluated placing O at `[4][16]` (R4 in notation).
2. `evaluate_threat_fast()` returned 100,000 (closed four: O O O O at
   cols 13-16, blocked on left by X at col 12).
3. Since 100,000 >= 100,000, the move was classified as a "winning move".
4. Step 1 returned R4 immediately. **Only 1 move was evaluated.**
5. Step 2 (blocking opponent threats) **never ran**.

#### What Should Have Happened

Step 2 would have evaluated the opponent's (X's) threat at each candidate
position. At position `[6][16]` (R6):

- `evaluate_threat_fast(board, 6, 16, AI_CELL_CROSSES, 19)` would return
  **1,000,000** (five-in-a-row: X X X X X at cols 12-16).
- This is >= 40,000, so it would be added to `blocking_moves`.
- The AI would have blocked at `[6][16]` instead of playing `[4][16]`.

#### Result

O played R4 (creating its own closed four). X then played R6 and won
with five-in-a-row at row 6, cols 12-16.

### Proposed Fix

Change the threshold in Step 1 from `100,000` to `500,000`:

```c
// STEP 1: Check for immediate winning moves
// Only open fours (500,000) and five-in-a-row (1,000,000) are unblockable
if (threat >= 500000) {
```

This ensures:
- Five-in-a-row (1,000,000) still triggers immediate play (correct).
- Open four (500,000) still triggers immediate play (opponent cannot block
  both ends, so this is a guaranteed win).
- Closed four (100,000) falls through to Step 2, which checks opponent
  threats before committing.

Additionally, `get_move_priority_optimized()` has the same threshold at
line ~162 and should be updated for consistency:

```c
if (my_threat >= 500000) {
    return 2000000000;
}
if (opp_threat >= 500000) {
    return 1500000000;
}
```

### Additional Concern: Open Four vs Opponent Five-in-a-Row

Even with the threshold raised to 500,000, there is an edge case: if the
AI creates an open four but the opponent can complete five-in-a-row with
their next move, the opponent wins first. However, this scenario is
extremely unlikely because the opponent's five-in-a-row opportunity would
have been completed on the opponent's previous turn. For safety, the fix
could also interleave Step 1 and Step 2:

1. Check for AI's five-in-a-row (1,000,000) — play immediately.
2. Check for opponent's five-in-a-row (1,000,000) — block immediately.
3. Check for AI's open four (500,000) — play it.
4. Check for opponent's open four / compound (>= 40,000) — block it.

This ordering ensures the AI never ignores a must-block threat.

### Failing Tests

See `tests/gomoku_test.cpp`:
- `AIBlocksOpponentClosedFour` — reproduces the exact bug scenario
- `AIBlocksFiveInARowOverOwnClosedFour` — verifies blocking priority
- `AIPlaysImmediateWinOverBlocking` — verifies actual wins still win

---

## BUG-002: Notation Inconsistency Between json_api.c and board.c

**Status**: Identified, not yet fixed (deferred — cosmetic, internally consistent)
**Severity**: Medium — causes confusion, potential for client-server mismatch
**Files**: `src/net/json_api.c` (lines 44-74), `src/gomoku/board.c` (lines 80-130)

### Summary

Two separate notation systems exist:

| Function | File | Row in "N6" | System |
|----------|------|-------------|--------|
| `coord_to_notation()` | json_api.c | row = 6 (0-based) | `"%c%d", col_letter, x` |
| `board_coord_to_notation()` | board.c | row = 5 (1-based) | `"%c%d", col_letter, row_x + 1` |

The JSON API is internally consistent (0-based in/out), and the TUI sidebar
uses 1-based for human-readable display. But having two incompatible notation
systems is a source of confusion and potential bugs if code ever mixes them.

### Impact

If a client sends notation using 1-based rows (as standard Go notation
expects) to the JSON API, moves will be placed one row off. The API's
`notation_to_coord()` interprets "N6" as row 6, but standard Go notation
"N6" means the 6th row (1-based = row index 5).

### Proposed Fix

Standardize on 1-based notation everywhere (matching Go/Gomoku convention):

1. Update `coord_to_notation()` in json_api.c to use `x + 1`.
2. Update `notation_to_coord()` in json_api.c to use `row - 1`.
3. Or, replace both json_api.c functions with calls to the board.c functions.

---

## BUG-003: Step 2b (Compound Three) Bypasses Defensive Checks

**Status**: Fixed
**Severity**: Medium — could cause AI to ignore opponent forced wins
**File**: `src/gomoku/ai.c`, Step 2b (moved after Step 4)

### Summary

Step 2b plays the AI's own compound three (score 30,000-39,999) without
checking if the opponent has an urgent response available. If the AI creates
a compound three but the opponent has a forced win (VCT) or a strong
counter-threat, the AI ignores it.

The step ordering is:
- Step 2: Block opponent >= 40,000 ✓
- Step 2b: Create AI compound three (30,000-40,000) — **no defense check**
- Step 3: Offensive VCT
- Step 4: Defensive VCT — **not reached if Step 2b fires**

### Proposed Fix

Move Step 2b after Step 4 (defensive VCT), or add an opponent threat check
within Step 2b before committing.
