# Gomoku Variant Support — Design Proposal

## Recommended Variants

Based on popularity, tournament use, and implementation complexity, here are the variants ranked by priority:

| Priority | Variant ID       | Name                    | Rationale |
|----------|------------------|-------------------------|-----------|
| 1        | `standard`       | Standard Gomoku         | **Default.** Exactly 5 wins, overlines don't count. This is what the engine already does. Most widely played casual variant worldwide. |
| 2        | `freestyle`      | Freestyle Gomoku        | Simplest change — 5+ in a row wins. Very popular online. |
| 3        | `renju`          | Renju                   | Professional tournament variant. Adds forbidden moves (3×3, 4×4, overline) for Black only. High complexity to implement but highly prestigious. |
| 4        | `caro`           | Caro (Gomoku+)          | Popular in Vietnam. Five in a row must have at least one open end. Moderate complexity. |
| 5        | `omok`           | Omok                    | Korean variant. Overlines don't count + three-and-three restriction for both players. |
| 6        | `ninuki`         | Ninuki-Renju / Pente    | Capture variant. Major engine changes needed (stone removal). Lower priority. |

### Why `standard` Should Be the Default

1. **Already implemented** — the current engine uses `count == 5` (exact five), which is standard gomoku.
2. **Most widely recognized** — when people say "gomoku" without qualification, they mean standard.
3. **Good balance** — overline restriction adds strategic depth without the complexity of forbidden moves.
4. **Backward compatible** — existing saved games and API clients continue to work unchanged.

---

## CLI Flag Design

### New Flag: `-g | --game-type`

```
-g, --game-type TYPE    Game variant (default: standard)
                        Variants: standard, freestyle, renju, caro, omok, ninuki
```

### Full CLI Integration

```bash
# Default (standard gomoku, exactly as current behavior)
./gomoku

# Freestyle — 5+ in a row counts
./gomoku -g freestyle

# Renju — forbidden moves for Black
./gomoku -g renju

# Caro — blocked five doesn't count
./gomoku -g caro

# Can combine with all existing flags
./gomoku -g renju -d 4 -b 15 -r 3 -x human -o ai -j game.json
```

### CLI Config Extension

```c
// Add to cli_config_t in src/gomoku/cli.h
typedef enum {
    GAME_STANDARD = 0,   // Exactly 5 wins, overline doesn't count (default)
    GAME_FREESTYLE,       // 5+ wins
    GAME_RENJU,           // Forbidden moves for Black (3x3, 4x4, overline)
    GAME_CARO,            // 5 must have at least one open end
    GAME_OMOK,            // Standard + 3x3 forbidden for both
    GAME_NINUKI,          // Capture variant
} game_type_t;

typedef struct {
    // ... existing fields ...
    game_type_t game_type;  // NEW: Game variant
} cli_config_t;
```

### Long-option Registration

```c
// Add to the getopt_long options array in src/gomoku/cli.c
{"game-type", required_argument, 0, 'g'},
```

Option string changes from `"d:l:t:b:r:j:p:w:uU:sqx:o:h"` to `"d:l:t:b:r:j:p:w:uU:sqx:o:g:h"`.

---

## JSON Schema Extension

### New Field: `game_type`

Add to the root properties in `config/gomoku-json-schema.json`:

```json
"game_type": {
  "type": "string",
  "enum": ["standard", "freestyle", "renju", "caro", "omok", "ninuki"],
  "default": "standard",
  "description": "Game variant determining win conditions and move restrictions. Defaults to 'standard' if omitted."
}
```

### Complete JSON Example

```json
{
  "X": { "player": "human" },
  "O": { "player": "AI", "depth": 4 },
  "board_size": 15,
  "game_type": "renju",
  "radius": 2,
  "timeout": "none",
  "winner": "none",
  "moves": [
    { "X (human)": "H8", "time_ms": 1200.0 }
  ]
}
```

### Backward Compatibility

When `game_type` is absent from the JSON, the engine treats it as `"standard"`. All existing saved games and API clients continue to work unchanged.

---

## Rule Matrix by Variant

| Rule                      | standard | freestyle | renju       | caro     | omok     | ninuki   |
|---------------------------|----------|-----------|-------------|----------|----------|----------|
| Board size                | 15/19    | 15/19     | 15          | 15/19    | 15       | 15/19    |
| Win count                 | =5       | >=5       | =5          | =5       | =5       | =5 or capture |
| Overline counts           | No       | Yes       | Black: lose, White: win | No | No | No |
| 3×3 forbidden             | No       | No        | Black only  | No       | Both     | No       |
| 4×4 forbidden             | No       | No        | Black only  | No       | No       | No       |
| Blocked-five rule         | No       | No        | No          | Yes      | No       | No       |
| Custodial capture         | No       | No        | No          | No       | No       | Yes      |
| Opening rules             | Any      | Any       | Tournament  | Any      | Any      | Any      |

---

## Implementation Strategy

### Phase 1: Core Infrastructure (Low Risk)

1. Add `game_type_t` enum and field to `cli_config_t` and `game_state_t`
2. Add `-g | --game-type` CLI flag parsing
3. Add `game_type` to JSON serialization/deserialization (both `game.c` and `json_api.c`)
4. Add `game_type` to the JSON schema
5. Wire `game_type` through to the existing `has_winner()` function

### Phase 2: Freestyle (Trivial)

Change `has_winner()` to accept `>=5` when `game_type == GAME_FREESTYLE`:

```c
if (game_type == GAME_FREESTYLE) {
    if (count >= 5) return 1;
} else {
    if (count == 5) return 1;
}
```

### Phase 3: Caro (Moderate)

Add open-end check to `has_winner()`:

```c
if (game_type == GAME_CARO && count == 5) {
    // Check if at least one end is open (not blocked by opponent or edge)
    int open_ends = 0;
    if (pos_end is empty) open_ends++;
    if (neg_end is empty) open_ends++;
    if (open_ends == 0) continue; // Both blocked — not a win
}
```

### Phase 4: Renju (Complex)

Requires new functions for forbidden move detection:

```c
// New functions needed in gomoku.c or a new renju.c module:
int is_overline(int **board, int size, int row, int col, int player);
int count_open_threes_at(int **board, int size, int row, int col, int player);
int count_fours_at(int **board, int size, int row, int col, int player);
int is_forbidden_move(int **board, int size, int row, int col, int player, game_type_t type);

// Modify has_winner for Renju:
// - Black wins only with exactly 5
// - White wins with 5 or overline
// - If Black plays a forbidden move, White wins

// Modify AI to exclude forbidden moves from candidate list
// Modify move validation to reject forbidden moves for human Black
```

The forbidden-move detection for Renju is the most complex part because of the recursive nature of the rules (a 3×3 might not be forbidden if one of the threes can't become an open four due to _another_ forbidden point).

### Phase 5: Omok (Moderate)

Similar to Renju but simpler — only the 3×3 restriction, applied to both players.

### Phase 6: Ninuki (Major)

Requires engine-level changes for stone removal:
- Add capture detection after each move
- Add stone removal from board
- Track capture count per player
- Add alternative win condition (5 captures = win)

---

## Files Requiring Changes

| File | Change |
|------|--------|
| `src/gomoku/gomoku.h`  | Add `game_type_t` enum |
| `src/gomoku/cli.h`     | Add `game_type` to `cli_config_t` |
| `src/gomoku/cli.c`     | Parse `-g` flag, add to help text |
| `src/gomoku/game.h`    | Add `game_type` to `game_state_t` |
| `src/gomoku/game.c`    | Serialize/deserialize `game_type` in JSON |
| `src/gomoku/gomoku.c`  | Make `has_winner()` variant-aware, add forbidden-move checks |
| `src/gomoku/ai.c`      | Filter forbidden moves from AI candidates (Renju) |
| `src/net/json_api.c`   | Parse/serialize `game_type` in HTTP JSON |
| `src/net/json_api.h`   | (if needed for type defs) |
| `config/gomoku-json-schema.json` | Add `game_type` field |
| `frontend/src/...`     | Add variant selector to settings panel |

---

## Schema Fixes Applied in This Session

Two bugs were found and fixed in `src/gomoku/game.c`:

| Bug | Before | After |
|-----|--------|-------|
| Field name mismatch | `"board": 15` | `"board_size": 15` (matches schema) |
| Type mismatch | `"undo": "on"` (string) | `"undo": true` (boolean, matches schema) |

The deserialization was also updated to accept both `"board_size"` and legacy `"board"` for backward compatibility.

The old `config/sample-game.json` (225-move draw, old field names) was replaced with a fresh 31-move AI-vs-AI game that validates cleanly against the schema.
