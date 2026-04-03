# AI Engine — Algorithm Analysis and Known Issues

A deep analysis of the Gomoku AI engine in `gomoku-c/src/gomoku/ai.c` and
`gomoku-c/src/gomoku/gomoku.c`, covering architecture, scoring, known issues,
and recommendations.

## 1. Architecture Overview

The AI move selection in `find_best_ai_move()` uses a **multi-step
decision pipeline** followed by minimax alpha-beta search. The steps
are evaluated sequentially; the first step that produces a decisive
result short-circuits all subsequent steps.

### Step Pipeline

| Step | Purpose | Threshold | Can Short-Circuit? |
|------|---------|-----------|-------------------|
| 1 | AI's own "winning" move | `my_threat >= 100000` | Yes (returns immediately) |
| 2 | Block opponent threat | `opp_threat >= 40000` | Yes |
| 2b | Create compound three | `my_threat in [30000, 40000)` | Yes |
| 3 | Offensive VCT (forced win) | VCT depth 10 | Yes |
| 4 | Defensive VCT (block forced win) | VCT depth 10 | Yes |
| 5 | Block opponent open three | `opp_threat == 1500 or in [30000, 40000)` | Yes (if no initiative) |
| 6 | Play forcing four | `my_threat >= 10000` | Yes |
| 7 | Minimax iterative deepening | depth 1..max_depth | Fallback |

### Key Observation

Each step acts as a hard gate. If Step N fires, Steps N+1 through 7
are never evaluated. This creates ordering dependencies where an
aggressive (offensive) step can prevent a defensive step from running.

## 2. Threat Scoring Table

Computed by `evaluate_threat_fast()` in `gomoku-c/src/gomoku/ai.c`.

### Per-Direction Scores

| Pattern | Contiguous | Open Ends | Score | Category |
|---------|-----------|-----------|-------|----------|
| Five+ in a row | >= 5 | any | 1,000,000 | Instant win |
| Open four | 4 | 2 | 500,000 | Guaranteed win (unblockable) |
| Closed four | 4 | 1 | 100,000 | Forcing (opponent must block) |
| Dead four | 4 | 0 | 0 | No threat (both ends blocked) |
| Gapped four | total >= 4, holes <= 1 | any | 8,000 | Forcing (like XX_XX) |
| Open three | 3 | 2 | 1,500 | Serious developing threat |
| Closed three | 3 | 1 | 500 | Weak threat |
| Broken three | total >= 3, holes <= 1 | >= 1 | 400 | Gapped threat |
| Open two | 2 | 2 | 100 | Early development |

### Compound (Cross-Direction) Bonuses

| Combination | Score | Rationale |
|-------------|-------|-----------|
| Two fours | 48,000 | Opponent can only block one direction |
| Four + three | 45,000 | Opponent can only block one threat |
| Double open three | 40,000 | Opponent can only block one three |
| Open three + two threes | 30,000 | Developing compound |
| Open two + open three | 3,000 | Building towards compound |
| Two open twos | 2,000 | Developing position |

### Score Ranges and Their Meaning

```
1,000,000  = Actual win (5 in a row on the board)
  500,000  = Open four (guaranteed win next move, cannot be blocked)
  100,000  = Closed four (must block, but CAN be blocked)
   48,000  = Double four (nearly winning compound)
   40,000+ = Compound threats (opponent can only block one)
    8,000  = Gapped four (forcing move)
    1,500  = Open three (needs attention)
      500  = Closed three
      400  = Broken three
      100  = Open two
```

## 3. Minimax Implementation

### 3.1 Search Structure

The minimax uses alpha-beta pruning with:
- **Transposition table** (Zobrist hashing)
- **Killer move heuristic** (depth-local)
- **Move ordering** via `get_move_priority_optimized()`
- **Iterative deepening** (depth 1 to max_depth)

### 3.2 Depth Parity Analysis

The user raised whether minimax depth should be odd or even. Here is
the detailed analysis.

#### Even Depth (e.g., 4)

```
Ply 1: AI moves       (maximizing)
Ply 2: Opponent moves  (minimizing)
Ply 3: AI moves       (maximizing)
Ply 4: Opponent moves  (minimizing)
       → Evaluate position (after opponent's move)
```

**Properties:**
- The search tree ends with the opponent's response.
- The AI can see whether the opponent blocks its threats.
- The AI CANNOT see its own winning response at ply 5.
- Better for **defensive** assessment.

#### Odd Depth (e.g., 3 or 5)

```
Ply 1: AI moves       (maximizing)
Ply 2: Opponent moves  (minimizing)
Ply 3: AI moves       (maximizing)
       → Evaluate position (after AI's own move)
```

**Properties:**
- The search tree ends with the AI's own move.
- The AI can see its own winning continuation.
- The AI CANNOT see the opponent's response to its final ply.
- Better for **offensive** assessment.

#### Depth 5 (Recommended)

```
Ply 1: AI moves       (maximizing)
Ply 2: Opponent moves  (minimizing)
Ply 3: AI moves       (maximizing)
Ply 4: Opponent moves  (minimizing)
Ply 5: AI moves       (maximizing)
       → Evaluate position (after AI's own move)
```

This is generally preferred because:
1. The AI sees one more ply than depth 4 (5 vs 4 half-moves).
2. The final ply is the AI's own move, so forced wins at ply 5 are
   detected within the search tree (not just by the static evaluator).
3. The static evaluator only needs to assess the resulting position,
   not predict whether the AI has a winning follow-up.

#### Practical Impact

For the specific BUG-001 (closed four bug), depth parity is NOT the
root cause — the bug occurs in the pre-minimax Step 1, which bypasses
minimax entirely. However, depth parity affects the quality of Step 7
(minimax) decisions:

- **Depth 4**: The AI might miss a winning continuation at ply 5.
  The static evaluator scores the position but may not see the
  forced win.
- **Depth 5**: The AI's own winning move at ply 5 is found during
  search, returning a WIN_SCORE, which is much more reliable.

#### Recommendation

Default depth should be **odd** (3 or 5). The current default of 4
should be changed to 5 for the AI's "normal" difficulty, and 3 for
"easy". This change makes the AI both stronger and more predictable
in its play.

### 3.3 Terminal Detection

The minimax has two levels of terminal detection:

1. **Fast path** (line ~766): `is_five_from_last_move()` checks if
   the last placed stone completed exactly five in a row. This is
   O(1) per position and runs before depth check.

2. **Fallback path** (line ~783): `get_cached_winner()` for callers
   that don't provide a valid last-move context. This is a full
   board scan.

The fast path correctly uses `count == 5` (not `count >= 5`),
enforcing the overline rule (six or more in a row does NOT win).

### 3.4 Static Evaluation at Leaf Nodes

At depth 0, the search calls `evaluate_position_incremental_fast()`
which evaluates only positions within radius 3 of the last move.

**Potential issue**: Threats far from the last move (e.g., an opponent
building a sequence on the other side of the board) are invisible to
the leaf evaluator. This is mitigated by:
- The pre-minimax steps (1-6) which scan ALL candidate moves.
- The move ordering which puts high-threat moves first.
- The search radius in move generation.

However, in games where the action spans multiple board regions
simultaneously, the local-region evaluation may miss critical
distant threats.

## 4. Move Generation and Search Radius

### How Candidates Are Generated

`generate_moves_optimized()` marks all empty cells within
`game->search_radius` squares of ANY occupied cell as candidates.

```
search_radius = 2 (default)

For each occupied cell at (x, y):
    All empty cells in [x-2, x+2] x [y-2, y+2] are candidates
```

### Impact of Radius

| Radius | Candidates (mid-game) | Behavior |
|--------|----------------------|----------|
| 1 | Very few | Misses most blocking moves |
| 2 | Moderate (default) | Good balance for most positions |
| 3 | Many | Better defense, slower search |
| 4 | Very many | Thorough but potentially too slow |

A radius of 2 means a blocking move must be within 2 cells of an
existing stone. In most Gomoku positions this is sufficient, but
in rare cases where stones are spread across the board, critical
blocking positions could fall outside the radius.

## 5. Move Priority Ordering

`get_move_priority_optimized()` assigns priorities for move ordering
in both pre-minimax steps and within the minimax search.

| Condition | Priority | Notes |
|-----------|----------|-------|
| `my_threat >= 100000` | 2,000,000,000 | **(BUG: includes closed fours)** |
| `opp_threat >= 100000` | 1,500,000,000 | **(BUG: same threshold issue)** |
| `my_threat >= 40000` | 1,200,000,000 + threat | Compound threats |
| `opp_threat >= 40000` | 1,100,000,000 + threat | Block compound threats |
| Killer move | +1,000,000 | Depth-local heuristic |
| `opp_threat >= 1500` | `my*10 + opp*12` | Defensive weighting |
| `opp_threat < 1500` | `my*15 + opp*5` | Offensive bias (3:1 ratio) |

### Offensive Bias

When the opponent has no urgent threats (< 1500), the AI weights its
own threats 3x more than the opponent's (15 vs 5). This intentional
bias favors attack over defense in quiet positions. It works well
when the AI has initiative but can backfire when subtle multi-move
threats are developing.

## 6. VCT (Victory by Continuous Threats)

### Offensive VCT (Step 3)

`find_forced_win()` searches for a sequence of forcing moves
(creating fours) that lead to an unstoppable compound threat.

**Algorithm**: For each candidate move that creates a four (threat >= 8000):
1. Place the stone.
2. If it creates a compound threat (>= 40000), it's a forced win.
3. Find the ONE cell the opponent must block.
4. Place the opponent's block.
5. Recurse (up to depth 10).

**Threshold issue at line ~545**: `if (post_threat >= 100000)` is used
to detect a "direct win" after placing a VCT stone. This has the same
100,000 threshold problem as Step 1 — a closed four is not a direct win.

### Defensive VCT (Step 4)

`find_forced_win_block()` checks if the opponent has a VCT and tries
to find a disrupting move. This runs AFTER Step 2b (compound three),
so if the AI plays a compound three in Step 2b, it never checks
whether the opponent has a forced win.

## 7. Step Ordering Issues

### Issue 1: Step 1 Threshold (Critical — BUG-001)

Step 1 treats `threat >= 100,000` (closed four) as an immediate win.
See BUG-001 below for full analysis.

### Issue 2: Step 2b Before Defensive VCT

```
Step 2b: Create compound three (30,000-40,000) → returns immediately
Step 3:  Offensive VCT (never reached)
Step 4:  Defensive VCT (never reached)
```

If the AI has a compound three but the opponent has a forced win (VCT),
the AI plays the compound three and ignores the opponent's forced win.

**Fix**: Move Step 2b after Step 4, or add an opponent-VCT check within
Step 2b before committing.

### Issue 3: Step 6 Without Defense Check

Step 6 plays the AI's own forcing four (`my_threat >= 10000`) without
checking whether the opponent has an urgent response. Steps 2-5 handle
most urgent defenses, but edge cases exist where Step 5 doesn't fire
(e.g., when the AI has "initiative") and Step 6 ignores the defense.

### Recommended Step Ordering

```
1.  AI five-in-a-row (threat >= 1,000,000)    — play and win
2.  Opponent five-in-a-row (opp >= 1,000,000)  — must block
3.  AI open four (threat >= 500,000)            — guaranteed win
4.  Opponent open four / compound (opp >= 40,000) — must block
5.  AI compound threat (threat >= 40,000)       — nearly winning
6.  Defensive VCT (opponent forced win)         — must disrupt
7.  Offensive VCT (AI forced win)               — play it
8.  Block opponent open three (1,500)           — if no initiative
9.  AI forcing four (threat >= 10,000)          — create pressure
10. Minimax search                              — general evaluation
```

This ordering ensures defense always trumps offense at each priority
level, while still allowing the AI to play winning moves immediately.

## 8. Additional Observations

### 8.1 Notation Inconsistency

Two separate notation functions exist with different row numbering:

- `gomoku-c/src/net/json_api.c:coord_to_notation()`: 0-based rows (`"%c%d", col, x`)
- `gomoku-c/src/gomoku/board.c:board_coord_to_notation()`: 1-based rows (`"%c%d", col, x+1`)

These are never mixed in practice (JSON API uses its own, TUI uses
board.c's), but the inconsistency is confusing and error-prone.
See BUG-002 below.

### 8.2 Overline Rule

The engine correctly implements the standard Gomoku overline rule:
exactly five in a row wins, six or more does not. This is enforced in
both `is_five_from_last_move()` (which checks `count == 5`) and
`has_winner()`.

### 8.3 Local Region Evaluation

`evaluate_position_incremental_fast()` (used at depth=0 in minimax)
does NOT check for wins/losses — it goes straight to the local-region
scoring. This is acceptable because the terminal check at lines 766-779
catches five-in-a-row before depth 0 is reached. However, it means the
leaf evaluation may miss NEAR-wins (four-in-a-row) that are outside
the eval radius of 3.

### 8.4 Random Tie-Breaking

When multiple moves have equal scores (in Steps 1, 2, and 7), one is
selected at random (`rand() % count`). This adds variety but can
occasionally choose a suboptimal move when the "equal" moves have
different strategic value not captured by the scoring.

## 9. Test Coverage Gaps

The following scenarios lack test coverage:

1. **AI must block opponent's closed four over creating own closed four**
   — This is the exact BUG-001 scenario.

2. **Step ordering: compound three vs defensive VCT** — Step 2b vs Step 4
   priority conflict.

3. **Even vs odd depth minimax quality** — No tests compare move quality
   at different search depths.

4. **Search radius edge cases** — No tests verify that blocking moves
   just at the radius boundary are found.

5. **VCT threshold issues** — `post_threat >= 100000` in
   `find_forced_win_recursive` may prematurely declare victory.

---

## 10. Known Bugs

### BUG-001: Closed Four Misclassified as Immediate Win (Fixed)

**Status**: Fixed
**Severity**: Critical — caused the AI to lose games it should win or draw
**File**: `gomoku-c/src/gomoku/ai.c`, function `find_best_ai_move()`
**Affected Lines**: ~1090 (Step 1 threshold), ~162 (move priority ordering), ~545 (VCT)

#### Summary

The AI's top-level move selection (Step 1 in `find_best_ai_move`) treats a
**closed four** (threat score 100,000) as an "immediate winning move" and
returns it without checking whether the opponent has a more urgent threat.
A closed four is blockable — the opponent can play at the single open end.
Only an open four (500,000) or an actual five-in-a-row (1,000,000) are
truly unblockable.

#### Root Cause

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

#### Reproduction: Game 2026-03-02T09-04-05

Recorded in `gomoku-game-2026-03-02T09-04-05.json`. The critical moment is
at move index 31 (O's turn, the AI playing as O with depth=4, radius=2).

##### Board State Before Move 31

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

##### What Happened

1. Step 1 evaluated placing O at `[4][16]` (R4 in notation).
2. `evaluate_threat_fast()` returned 100,000 (closed four: O O O O at
   cols 13-16, blocked on left by X at col 12).
3. Since 100,000 >= 100,000, the move was classified as a "winning move".
4. Step 1 returned R4 immediately. **Only 1 move was evaluated.**
5. Step 2 (blocking opponent threats) **never ran**.

##### What Should Have Happened

Step 2 would have evaluated the opponent's (X's) threat at each candidate
position. At position `[6][16]` (R6):

- `evaluate_threat_fast(board, 6, 16, AI_CELL_CROSSES, 19)` would return
  **1,000,000** (five-in-a-row: X X X X X at cols 12-16).
- This is >= 40,000, so it would be added to `blocking_moves`.
- The AI would have blocked at `[6][16]` instead of playing `[4][16]`.

##### Result

O played R4 (creating its own closed four). X then played R6 and won
with five-in-a-row at row 6, cols 12-16.

#### Fix Applied

Changed the threshold in Step 1 from `100,000` to `500,000`:

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
line ~162 and was updated for consistency:

```c
if (my_threat >= 500000) {
    return 2000000000;
}
if (opp_threat >= 500000) {
    return 1500000000;
}
```

#### Additional Concern: Open Four vs Opponent Five-in-a-Row

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

#### Failing Tests

See `gomoku-c/tests/gomoku_test.cpp`:
- `AIBlocksOpponentClosedFour` — reproduces the exact bug scenario
- `AIBlocksFiveInARowOverOwnClosedFour` — verifies blocking priority
- `AIPlaysImmediateWinOverBlocking` — verifies actual wins still win

---

### BUG-002: Notation Inconsistency Between json_api.c and board.c

**Status**: Identified, not yet fixed (deferred — cosmetic, internally consistent)
**Severity**: Medium — causes confusion, potential for client-server mismatch
**Files**: `gomoku-c/src/net/json_api.c` (lines 44-74), `gomoku-c/src/gomoku/board.c` (lines 80-130)

#### Summary

Two separate notation systems exist:

| Function | File | Row in "N6" | System |
|----------|------|-------------|--------|
| `coord_to_notation()` | json_api.c | row = 6 (0-based) | `"%c%d", col_letter, x` |
| `board_coord_to_notation()` | board.c | row = 5 (1-based) | `"%c%d", col_letter, row_x + 1` |

The JSON API is internally consistent (0-based in/out), and the TUI sidebar
uses 1-based for human-readable display. But having two incompatible notation
systems is a source of confusion and potential bugs if code ever mixes them.

#### Impact

If a client sends notation using 1-based rows (as standard Go notation
expects) to the JSON API, moves will be placed one row off. The API's
`notation_to_coord()` interprets "N6" as row 6, but standard Go notation
"N6" means the 6th row (1-based = row index 5).

#### Proposed Fix

Standardize on 1-based notation everywhere (matching Go/Gomoku convention):

1. Update `coord_to_notation()` in json_api.c to use `x + 1`.
2. Update `notation_to_coord()` in json_api.c to use `row - 1`.
3. Or, replace both json_api.c functions with calls to the board.c functions.

---

### BUG-003: Step 2b (Compound Three) Bypasses Defensive Checks (Fixed)

**Status**: Fixed
**Severity**: Medium — could cause AI to ignore opponent forced wins
**File**: `gomoku-c/src/gomoku/ai.c`, Step 2b (moved after Step 4)

#### Summary

Step 2b plays the AI's own compound three (score 30,000-39,999) without
checking if the opponent has an urgent response available. If the AI creates
a compound three but the opponent has a forced win (VCT) or a strong
counter-threat, the AI ignores it.

The step ordering is:
- Step 2: Block opponent >= 40,000 ✓
- Step 2b: Create AI compound three (30,000-40,000) — **no defense check**
- Step 3: Offensive VCT
- Step 4: Defensive VCT — **not reached if Step 2b fires**

#### Fix Applied

Moved Step 2b after Step 4 (defensive VCT), ensuring the AI checks for
opponent forced wins before committing to a compound three.

---

## 11. Strategic Evaluation Improvements (TODO)

### Problem: Horizon Effect / Loss of Initiative

The AI plays tactically well (blocking immediate threats, finding forced wins) but
lacks strategic awareness. Analysis of `games/manual-game.json` (human beat AI at
depth 5) revealed:

- After O's initial diagonal attack was blocked at both ends (moves 2-9), O had no
  fallback plan. Move 10 was a purely defensive block (score=100) that surrendered
  the initiative permanently.
- From move 14-24, every O move was blocking. Pure defense always loses in gomoku
  because the attacker builds threats faster than the defender can block.
- X quietly built an anti-diagonal ([6][7],[5][8],[4][9],[3][10]) over 4 moves while
  O was busy blocking other threats. O never saw the strategic danger.

### Root Cause

`evaluate_position_incremental_fast()` scores positions based on local patterns
(individual lines of stones) but does not account for:

1. **Initiative**: Having threats that force the opponent to respond is worth more
   than the threat score itself, because it gives you a free tempo to build elsewhere.
2. **Multi-directional pressure**: N threats in N different directions are
   exponentially more dangerous than N threats in the same direction. The opponent
   can only block one direction per move.
3. **Offensive potential**: A position with zero developing threats is strategically
   lost even if no single opponent threat is immediately lethal. The evaluation
   should penalize positions where one side has no offensive patterns.
4. **Threat diversity bonus**: Two open twos in different directions (diamond/fork
   setup) now score 25000 at the point of placement, but the evaluation function
   doesn't reward the *developing* position before the pivot is played.

### Proposed Fix

Add a strategic overlay to the evaluation function that:

1. Scans the board for all developing patterns (open twos, open threes) per player
2. Counts how many **independent directions** each player has active threats in
3. Applies a super-linear bonus for multi-directional threats:
   - 1 direction: baseline
   - 2 directions: 1.5x bonus
   - 3 directions: 3x bonus
   - 4 directions: 5x bonus
4. Adds a **pressure differential** term: if one side has 3+ developing threats and
   the other has 0, apply a large positional penalty/bonus (e.g. +/- 5000)
5. Penalizes positions where the AI has **no offensive potential** (no open twos or
   better), ensuring minimax prefers moves that maintain attacking options

### Files to Modify

- `gomoku-c/src/gomoku/ai.c`: `evaluate_position_incremental_fast()` — add strategic scan
- `gomoku-c/src/gomoku/gomoku.c`: `evaluate_position()` — full-board evaluation (used less
  frequently but should be consistent)
