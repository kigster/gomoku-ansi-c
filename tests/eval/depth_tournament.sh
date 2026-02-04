#!/bin/bash
#
# Depth Tournament - Measures AI strength across different search depths
#
# Usage: ./depth_tournament.sh [--games N] [--depths "2,3,4,5"]
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
GOMOKU="$ROOT_DIR/gomoku"
OUTPUT_DIR="$ROOT_DIR/tests/eval/results"

# Default parameters
GAMES_PER_MATCHUP=20
DEPTHS="2,3,4"
RADIUS=2
BOARD_SIZE=15

# Parse arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --games)
      GAMES_PER_MATCHUP="$2"
      shift 2
      ;;
    --depths)
      DEPTHS="$2"
      shift 2
      ;;
    --radius)
      RADIUS="$2"
      shift 2
      ;;
    --board)
      BOARD_SIZE="$2"
      shift 2
      ;;
    *)
      echo "Unknown option: $1"
      exit 1
      ;;
  esac
done

# Check if gomoku binary exists
if [[ ! -x "$GOMOKU" ]]; then
  echo "Error: gomoku binary not found at $GOMOKU"
  echo "Run 'make gomoku' first"
  exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULTS_FILE="$OUTPUT_DIR/tournament_$TIMESTAMP.txt"

# Parse depths into array
IFS=',' read -ra DEPTH_ARRAY <<< "$DEPTHS"

echo "==========================================="
echo "Gomoku AI Depth Tournament"
echo "==========================================="
echo "Games per matchup: $GAMES_PER_MATCHUP"
echo "Depths: ${DEPTH_ARRAY[*]}"
echo "Radius: $RADIUS"
echo "Board: ${BOARD_SIZE}x${BOARD_SIZE}"
echo "Results: $RESULTS_FILE"
echo "==========================================="
echo ""

# Initialize results array
declare -A WINS
declare -A LOSSES
declare -A DRAWS

for d in "${DEPTH_ARRAY[@]}"; do
  WINS[$d]=0
  LOSSES[$d]=0
  DRAWS[$d]=0
done

# Run tournament
for ((i=0; i<${#DEPTH_ARRAY[@]}; i++)); do
  for ((j=i+1; j<${#DEPTH_ARRAY[@]}; j++)); do
    DEPTH_X="${DEPTH_ARRAY[$i]}"
    DEPTH_O="${DEPTH_ARRAY[$j]}"

    echo "--- Matchup: Depth $DEPTH_X (X) vs Depth $DEPTH_O (O) ---"

    x_wins=0
    o_wins=0
    draws=0

    for ((g=1; g<=GAMES_PER_MATCHUP; g++)); do
      GAME_FILE="$OUTPUT_DIR/game_${DEPTH_X}v${DEPTH_O}_${g}.json"

      # Run game silently
      "$GOMOKU" -x ai -o ai \
        --depth-x "$DEPTH_X" --depth-o "$DEPTH_O" \
        -r "$RADIUS" -b "$BOARD_SIZE" \
        -s -j "$GAME_FILE" 2>/dev/null

      # Parse winner from JSON
      if [[ -f "$GAME_FILE" ]]; then
        winner=$(grep -o '"winner":"[^"]*"' "$GAME_FILE" | cut -d'"' -f4)
        case "$winner" in
          X)
            ((x_wins++))
            ;;
          O)
            ((o_wins++))
            ;;
          draw)
            ((draws++))
            ;;
        esac
        rm -f "$GAME_FILE"  # Clean up
      fi

      # Progress indicator
      printf "\r  Game %d/%d: X(d%d)=%d, O(d%d)=%d, Draw=%d" \
        "$g" "$GAMES_PER_MATCHUP" "$DEPTH_X" "$x_wins" "$DEPTH_O" "$o_wins" "$draws"
    done
    echo ""

    # Record results (X plays first, so adjust for first-move advantage)
    WINS[$DEPTH_X]=$((${WINS[$DEPTH_X]} + x_wins))
    LOSSES[$DEPTH_X]=$((${LOSSES[$DEPTH_X]} + o_wins))
    DRAWS[$DEPTH_X]=$((${DRAWS[$DEPTH_X]} + draws))

    WINS[$DEPTH_O]=$((${WINS[$DEPTH_O]} + o_wins))
    LOSSES[$DEPTH_O]=$((${LOSSES[$DEPTH_O]} + x_wins))
    DRAWS[$DEPTH_O]=$((${DRAWS[$DEPTH_O]} + draws))

    # Also run reverse matchup (swap who plays X)
    echo "--- Matchup: Depth $DEPTH_O (X) vs Depth $DEPTH_X (O) ---"

    x_wins_rev=0
    o_wins_rev=0
    draws_rev=0

    for ((g=1; g<=GAMES_PER_MATCHUP; g++)); do
      GAME_FILE="$OUTPUT_DIR/game_${DEPTH_O}v${DEPTH_X}_${g}.json"

      "$GOMOKU" -x ai -o ai \
        --depth-x "$DEPTH_O" --depth-o "$DEPTH_X" \
        -r "$RADIUS" -b "$BOARD_SIZE" \
        -s -j "$GAME_FILE" 2>/dev/null

      if [[ -f "$GAME_FILE" ]]; then
        winner=$(grep -o '"winner":"[^"]*"' "$GAME_FILE" | cut -d'"' -f4)
        case "$winner" in
          X)
            ((x_wins_rev++))
            ;;
          O)
            ((o_wins_rev++))
            ;;
          draw)
            ((draws_rev++))
            ;;
        esac
        rm -f "$GAME_FILE"
      fi

      printf "\r  Game %d/%d: X(d%d)=%d, O(d%d)=%d, Draw=%d" \
        "$g" "$GAMES_PER_MATCHUP" "$DEPTH_O" "$x_wins_rev" "$DEPTH_X" "$o_wins_rev" "$draws_rev"
    done
    echo ""

    WINS[$DEPTH_O]=$((${WINS[$DEPTH_O]} + x_wins_rev))
    LOSSES[$DEPTH_O]=$((${LOSSES[$DEPTH_O]} + o_wins_rev))
    DRAWS[$DEPTH_O]=$((${DRAWS[$DEPTH_O]} + draws_rev))

    WINS[$DEPTH_X]=$((${WINS[$DEPTH_X]} + o_wins_rev))
    LOSSES[$DEPTH_X]=$((${LOSSES[$DEPTH_X]} + x_wins_rev))
    DRAWS[$DEPTH_X]=$((${DRAWS[$DEPTH_X]} + draws_rev))

    echo ""
  done
done

# Print summary
echo "==========================================="
echo "TOURNAMENT RESULTS"
echo "==========================================="
echo ""
printf "%-8s %8s %8s %8s %8s\n" "Depth" "Wins" "Losses" "Draws" "Win%"
echo "-------------------------------------------"

for d in "${DEPTH_ARRAY[@]}"; do
  total=$((${WINS[$d]} + ${LOSSES[$d]} + ${DRAWS[$d]}))
  if [[ $total -gt 0 ]]; then
    win_pct=$(echo "scale=1; ${WINS[$d]} * 100 / $total" | bc)
  else
    win_pct="0.0"
  fi
  printf "%-8s %8d %8d %8d %7s%%\n" "D$d" "${WINS[$d]}" "${LOSSES[$d]}" "${DRAWS[$d]}" "$win_pct"
done

echo ""
echo "Results saved to: $RESULTS_FILE"

# Save detailed results
{
  echo "Tournament: $TIMESTAMP"
  echo "Games per matchup: $GAMES_PER_MATCHUP"
  echo "Depths: ${DEPTH_ARRAY[*]}"
  echo ""
  printf "%-8s %8s %8s %8s %8s\n" "Depth" "Wins" "Losses" "Draws" "Win%"
  for d in "${DEPTH_ARRAY[@]}"; do
    total=$((${WINS[$d]} + ${LOSSES[$d]} + ${DRAWS[$d]}))
    if [[ $total -gt 0 ]]; then
      win_pct=$(echo "scale=1; ${WINS[$d]} * 100 / $total" | bc)
    else
      win_pct="0.0"
    fi
    printf "%-8s %8d %8d %8d %7s%%\n" "D$d" "${WINS[$d]}" "${LOSSES[$d]}" "${DRAWS[$d]}" "$win_pct"
  done
} > "$RESULTS_FILE"

# Validation: Higher depth should win more
echo ""
echo "VALIDATION:"
prev_wins=0
all_passed=true
for d in "${DEPTH_ARRAY[@]}"; do
  if [[ ${WINS[$d]} -lt $prev_wins ]]; then
    echo "  WARNING: Depth $d has fewer wins than lower depth"
    all_passed=false
  fi
  prev_wins=${WINS[$d]}
done

if $all_passed; then
  echo "  PASS: Higher depths consistently beat lower depths"
fi
