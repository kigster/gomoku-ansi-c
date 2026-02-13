# Add VCT forced-win search, scoring reports, and narrowed blocking

## Summary

This PR adds a Victory by Continuous Threats (VCT) search to the gomoku AI, narrows the over-aggressive blocking heuristics, and introduces a scoring report system for AI decision transparency. It also adds `queue_wait_ms` passthrough in the JSON API, a `--report-scoring` flag for `gomoku-httpd`, an updated JSON schema, and CI validation for the schema.

## Description

### VCT (Victory by Continuous Threats) Search

Three new functions in `ai.c` implement a classic gomoku forced-win technique:

- **`find_block_cell()`** -- After placing a stone that creates a four, finds the single cell the opponent must block. Returns 0 for open fours (two block points = unstoppable).
- **`find_forced_win()`** -- Recursive threat-space search up to 10 moves deep. Explores forcing move sequences where each move creates a four, the opponent's response is deterministic (one block), and the sequence leads to an unstoppable position. Branching factor is ~3-5, so the search completes in milliseconds.
- **`find_forced_win_block()`** -- Defensive mirror: checks if the opponent can force a win, and finds the move that disrupts their sequence by trying each candidate move and re-running the opponent's VCT search.

### Narrowed Blocking Threshold

The previous heuristic treated any opponent threat >= 1500 (including 2000, 3000, 8000) as "unstoppable" and bypassed minimax to panic-block. This made the AI overly defensive. The new logic only emergency-blocks:

- `opp_threat == 1500` (true open three, only when AI has no initiative)
- `opp_threat >= 30000 && < 40000` (compound developing threats)
- `opp_threat >= 40000` (open four / double four, kept unchanged)

Scores 2000 (shared open twos), 3000 (developing), and 8000 (gapped four) now fall through to VCT search or minimax, which handles them more accurately.

### New `find_best_ai_move()` Flow

1. Check immediate win (threat >= 100000)
2. Block opponent >= 40000
3. **Offensive VCT** -- if we can force a win, play it
4. **Defensive VCT** -- if opponent can force a win, disrupt their sequence
5. Block open three (1500) only when we lack initiative
6. Play forcing four (>= 10000)
7. Minimax iterative deepening

### Scoring Report Infrastructure

Each evaluator step records a `scoring_entry_t` with: evaluator name, player perspective, moves evaluated, score, wall-clock time, decisive flag, and VCT sequence (when applicable). The `scoring_report_t` aggregates these entries plus `offensive_max_score` and `defensive_max_score`.

### HTTP API Changes

- **`-r / --report-scoring`** flag for `gomoku-httpd`: when enabled, the last AI move in JSON responses includes `offensive_max_score`, `defensive_max_score`, `think_time_ms`, and a `scoring` array with all evaluator results.
- **`queue_wait_ms`** passthrough: parsed from client request, preserved through to response JSON.
- **`json_api_serialize_game_ex()`**: new extended serialization function accepting an optional scoring report.

### JSON Schema and CI

- Updated `doc/gomoku-json-schema.json` with all new fields: `moves_evaluated`, `queue_wait_ms`, `offensive_max_score`, `defensive_max_score`, `think_time_ms`, `scoring` array, `scoring_entry` definition, `"draw"` winner value, and `timeout` as `oneOf` string/integer.
- Added `validate-schema` job to `.github/workflows/ci.yml` using `ajv-cli` to validate `doc/sample-game.json` against the schema on every push.

## Motivation

The AI's blocking heuristic was too aggressive -- it treated moderate threats (2000, 3000, 8000) as emergencies, skipping minimax entirely and playing purely defensive moves. This prevented the AI from finding tactical wins. VCT search fills the gap: before falling back to minimax, the AI now checks whether it can force a win through a sequence of fours, or whether the opponent can do the same. The scoring report infrastructure makes AI decisions transparent, enabling debugging and analysis of why the AI chose a particular move.

## Testing

- **24/24 game unit tests pass** (`test_gomoku`)
- **31/31 daemon unit tests pass** (`test_daemon`)
- **Functional HTTP tests**: verified move parsing, AI responses, scoring report JSON, VCT detection, and `queue_wait_ms` passthrough via curl against `gomoku-httpd`
- **Schema validation**: `doc/sample-game.json` validates against the updated schema using `ajv-cli`
- `make format` applied to all source files

## Backwards Compatibility

- `find_best_ai_move()` signature changed to accept `scoring_report_t *report` (pass `NULL` to opt out). All existing callers updated.
- `json_api_serialize_game()` unchanged (delegates to `_ex` with `NULL` report).
- JSON output fields `moves_searched` accepted as input alongside new `moves_evaluated` for backwards compatibility.
- `queue_wait_ms` and scoring fields are optional in both input and output.

## Scalability and Performance Impact

- **VCT search** has a small branching factor (~3-5 four-creating moves per level, 1 forced response) and runs to depth 10. Worst case ~59K nodes, completing in 1-15ms.
- **Defensive VCT** runs the offensive VCT for each candidate move (~50-100 moves), totaling ~100-500ms. Well within acceptable latency for a turn-based game.
- Scoring report overhead is negligible: a few struct writes per move.
- The narrowed blocking threshold may cause some positions to reach minimax that previously short-circuited, but these are positions where minimax produces better results.

## Code Quality Analysis

- All new functions follow existing code patterns and naming conventions.
- VCT functions properly undo board modifications (stone placement and removal) on all code paths.
- Scoring report uses stack-allocated fixed-size arrays (no dynamic allocation).
- `clang-format` applied to all modified files.
- No new compiler warnings.
