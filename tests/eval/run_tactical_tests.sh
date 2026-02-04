#!/bin/bash
#
# Tactical Test Runner - Verifies AI finds correct moves in known positions
#
# Usage: ./run_tactical_tests.sh [--verbose] [--test NAME]
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
HTTPD="$ROOT_DIR/gomoku-httpd"
TESTS_DIR="$SCRIPT_DIR/tactical_tests"

VERBOSE=false
SINGLE_TEST=""
PORT=18080

# Parse arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --verbose|-v)
      VERBOSE=true
      shift
      ;;
    --test|-t)
      SINGLE_TEST="$2"
      shift 2
      ;;
    --port|-p)
      PORT="$2"
      shift 2
      ;;
    *)
      echo "Unknown option: $1"
      exit 1
      ;;
  esac
done

# Check dependencies
if [[ ! -x "$HTTPD" ]]; then
  echo "Error: gomoku-httpd not found. Run 'make gomoku-httpd' first."
  exit 1
fi

if ! command -v curl &> /dev/null; then
  echo "Error: curl is required"
  exit 1
fi

if ! command -v jq &> /dev/null; then
  echo "Error: jq is required. Install with: brew install jq"
  exit 1
fi

# Start httpd in background
echo "Starting gomoku-httpd on port $PORT..."
pkill -f "gomoku-httpd.*$PORT" 2>/dev/null || true
sleep 0.5
"$HTTPD" -b "127.0.0.1:$PORT" -d
sleep 1

# Cleanup on exit
cleanup() {
  pkill -f "gomoku-httpd.*$PORT" 2>/dev/null || true
}
trap cleanup EXIT

# Run tests
passed=0
failed=0
total=0

echo ""
echo "==========================================="
echo "TACTICAL TEST SUITE"
echo "==========================================="
echo ""

for test_file in "$TESTS_DIR"/*.json; do
  [[ -f "$test_file" ]] || continue

  test_name=$(basename "$test_file" .json)

  # Skip if single test specified and this isn't it
  if [[ -n "$SINGLE_TEST" && "$test_name" != "$SINGLE_TEST" ]]; then
    continue
  fi

  ((total++))

  # Load test
  name=$(jq -r '.name' "$test_file")
  description=$(jq -r '.description' "$test_file")
  category=$(jq -r '.category' "$test_file")
  difficulty=$(jq -r '.difficulty' "$test_file")

  echo "Test: $name"
  echo "  Category: $category | Difficulty: $difficulty"
  echo "  $description"

  # Send request to get AI's move
  response=$(curl -s -X POST "http://127.0.0.1:$PORT/gomoku/play" \
    -H "Content-Type: application/json" \
    -d "@$test_file")

  if [[ $VERBOSE == true ]]; then
    echo "  Response: $response" | head -c 200
    echo ""
  fi

  # Extract AI's move from response
  # Find the last move in the response (the one AI just made)
  ai_move=$(echo "$response" | jq -r '.moves[-1] | to_entries[] | select(.value | type == "array") | .value | @json')

  if [[ -z "$ai_move" || "$ai_move" == "null" ]]; then
    echo "  ERROR: Could not extract AI move from response"
    ((failed++))
    echo ""
    continue
  fi

  # Parse move coordinates
  move_x=$(echo "$ai_move" | jq '.[0]')
  move_y=$(echo "$ai_move" | jq '.[1]')

  # Check expected moves
  expected_moves=$(jq -r '.expected_moves // empty' "$test_file")

  if [[ -n "$expected_moves" ]]; then
    # Check if AI's move is in expected moves
    match=$(echo "$expected_moves" | jq --argjson x "$move_x" --argjson y "$move_y" \
      'any(.[]; .[0] == $x and .[1] == $y)')

    if [[ "$match" == "true" ]]; then
      echo "  PASS: AI played [$move_x, $move_y] (expected)"
      ((passed++))
    else
      echo "  FAIL: AI played [$move_x, $move_y]"
      echo "        Expected one of: $expected_moves"
      ((failed++))
    fi
  fi

  # Check forbidden moves
  forbidden_moves=$(jq -r '.forbidden_moves // empty' "$test_file")

  if [[ -n "$forbidden_moves" ]]; then
    is_forbidden=$(echo "$forbidden_moves" | jq --argjson x "$move_x" --argjson y "$move_y" \
      'any(.[]; .[0] == $x and .[1] == $y)')

    if [[ "$is_forbidden" == "true" ]]; then
      echo "  FAIL: AI played forbidden move [$move_x, $move_y]"
      ((failed++))
    else
      # Check expected behavior
      expected_behavior=$(jq -r '.expected_behavior // empty' "$test_file")
      if [[ "$expected_behavior" == "offensive" ]]; then
        # For offensive tests, we just check it didn't play a forbidden move
        echo "  PASS: AI avoided defensive trap, played [$move_x, $move_y]"
        ((passed++))
      fi
    fi
  fi

  echo ""
done

# Summary
echo "==========================================="
echo "RESULTS"
echo "==========================================="
echo "Total:  $total"
echo "Passed: $passed"
echo "Failed: $failed"

if [[ $total -gt 0 ]]; then
  pct=$((passed * 100 / total))
  echo "Score:  $pct%"
fi

echo ""

if [[ $failed -gt 0 ]]; then
  echo "STATUS: SOME TESTS FAILED"
  exit 1
else
  echo "STATUS: ALL TESTS PASSED"
  exit 0
fi
