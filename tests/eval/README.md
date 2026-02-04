# Gomoku AI Evaluation Framework

This directory contains tools for measuring the strength and quality of the Gomoku AI.

## Evaluation Methods

### 1. Depth Tournament (`depth_tournament.sh`)

Runs a round-robin tournament between different AI depths to establish a strength hierarchy.

```bash
./tests/eval/depth_tournament.sh --games 50 --depths "2,3,4,5"
```

Expected results:
- Depth N should beat Depth N-1 with >60% win rate
- First-player advantage means X wins more often

### 2. Tactical Test Suite (`tactical_tests/`)

A collection of positions with known correct moves, categorized by:
- **Defense**: Must-block situations (open fours, open threes)
- **Offense**: Winning combinations (double threats, checkmates)
- **Positional**: Best developing moves

Run with:
```bash
./tests/eval/run_tactical_tests.sh
```

### 3. LLM Evaluation (`llm_eval.py`)

Uses an LLM to evaluate game quality by analyzing transcripts.

```bash
python tests/eval/llm_eval.py --game saved.json --model claude-3-opus
```

Outputs:
- Per-move quality scores (1-10)
- Identified blunders
- Overall game rating
- Suggested improvements

### 4. Regression Tests (`regression/`)

Golden game files that verify AI behavior hasn't degraded.

## Metrics

| Metric | Description | Target |
|--------|-------------|--------|
| Depth Win Rate | D(n) vs D(n-1) win % | >65% |
| Tactical Accuracy | % puzzles solved | >90% |
| Blunders/Game | Moves with >500 eval drop | <1 |
| Avg Move Quality | LLM rating 1-10 | >7 |

## Adding New Tests

### Tactical Position Format

```json
{
  "name": "block_double_three",
  "description": "O must block X's double-three threat",
  "board_size": 15,
  "board": [
    "...............",
    "...............",
    "......X........",
    ".....X.........",
    "....X..........",
    "...............",
    "...X...........",
    "...............",
    "..............."
  ],
  "current_player": "O",
  "expected_moves": [[4, 5]],
  "category": "defense",
  "difficulty": "medium"
}
```
