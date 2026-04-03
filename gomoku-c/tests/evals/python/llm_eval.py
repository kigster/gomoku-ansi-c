#!/usr/bin/env python3
"""
LLM-based Gomoku Game Evaluator

Analyzes game transcripts using an LLM to assess move quality and identify blunders.

Usage:
    python llm_eval.py --game saved.json [--api-key KEY] [--model MODEL]

Requires:
    pip install anthropic  # or openai
"""

import argparse
import json
import sys
from pathlib import Path

# Try to import API clients
try:
    import anthropic
    HAS_ANTHROPIC = True
except ImportError:
    HAS_ANTHROPIC = False

try:
    import openai
    HAS_OPENAI = True
except ImportError:
    HAS_OPENAI = False


EVALUATION_PROMPT = """You are an expert Gomoku (Five in a Row) analyst. Analyze the following game and evaluate each move.

Game Rules:
- Players alternate placing stones (X goes first, O second)
- First to get 5 in a row (horizontal, vertical, or diagonal) wins
- X typically has first-move advantage

Board coordinates are [row, col] starting from [0,0] at top-left.

GAME TRANSCRIPT:
{game_transcript}

FINAL RESULT: {winner}

Please analyze this game and provide:

1. **Overall Game Quality** (1-10):
   - 10 = Perfect/near-perfect play from both sides
   - 7-9 = Strong play with minor inaccuracies
   - 4-6 = Decent play with some mistakes
   - 1-3 = Weak play with significant errors

2. **Move-by-Move Analysis**:
   For each move, rate it 1-10 and briefly explain:
   - 10 = Optimal/winning move
   - 7-9 = Good move
   - 4-6 = Acceptable but not best
   - 1-3 = Mistake or blunder

3. **Critical Moments**:
   Identify the 2-3 most important moves that decided the game.

4. **Blunders**:
   List any moves that significantly worsened the position (rating 1-3).
   A blunder is a move that turns a winning/drawing position into a losing one.

5. **Improvement Suggestions**:
   For the losing side, suggest what they could have done differently.

Format your response as JSON:
{{
    "overall_rating": 7,
    "x_rating": 8,
    "o_rating": 6,
    "moves": [
        {{"move": 1, "player": "X", "position": [9, 9], "rating": 9, "comment": "Standard center opening"}},
        ...
    ],
    "critical_moments": [
        {{"move": 15, "description": "X creates unstoppable double-three threat"}}
    ],
    "blunders": [
        {{"move": 14, "player": "O", "position": [7, 6], "rating": 2, "explanation": "Blocked already-closed three instead of creating own threat"}}
    ],
    "suggestions": "O should have focused on creating own threats rather than passive defense"
}}
"""


def load_game(filepath: str) -> dict:
    """Load game JSON file."""
    with open(filepath, 'r') as f:
        return json.load(f)


def format_game_transcript(game: dict) -> str:
    """Format game data into readable transcript."""
    lines = []
    lines.append(f"Board Size: {game.get('board', game.get('board_size', 19))}x{game.get('board', game.get('board_size', 19))}")
    lines.append(f"X Player: {game.get('X', {}).get('player', 'unknown')}")
    lines.append(f"O Player: {game.get('O', {}).get('player', 'unknown')}")
    lines.append("")
    lines.append("Moves:")

    for i, move in enumerate(game.get('moves', []), 1):
        # Extract player and position from move object
        player = None
        position = None
        for key, value in move.items():
            if isinstance(value, list) and len(value) == 2:
                player = 'X' if 'X' in key else 'O'
                position = value
                break

        if player and position:
            time_ms = move.get('time_ms', 0)
            eval_count = move.get('moves_evaluated', move.get('moves_searched', 0))
            score = move.get('score', '')

            line = f"  {i}. {player} plays [{position[0]}, {position[1]}]"
            if time_ms:
                line += f" ({time_ms:.0f}ms)"
            if eval_count:
                line += f" [{eval_count} positions evaluated]"
            if score:
                line += f" (score: {score})"
            lines.append(line)

    return "\n".join(lines)


def evaluate_with_anthropic(game: dict, api_key: str, model: str = "claude-sonnet-4-20250514") -> dict:
    """Evaluate game using Anthropic Claude."""
    if not HAS_ANTHROPIC:
        raise ImportError("anthropic package not installed. Run: pip install anthropic")

    client = anthropic.Anthropic(api_key=api_key)

    transcript = format_game_transcript(game)
    winner = game.get('winner', 'unknown')

    prompt = EVALUATION_PROMPT.format(
        game_transcript=transcript,
        winner=winner
    )

    response = client.messages.create(
        model=model,
        max_tokens=4096,
        messages=[{"role": "user", "content": prompt}]
    )

    # Extract JSON from response
    response_text = response.content[0].text

    # Try to find JSON in response
    try:
        # Look for JSON block
        if "```json" in response_text:
            json_str = response_text.split("```json")[1].split("```")[0]
        elif "```" in response_text:
            json_str = response_text.split("```")[1].split("```")[0]
        else:
            json_str = response_text

        return json.loads(json_str)
    except json.JSONDecodeError:
        return {"raw_response": response_text, "parse_error": True}


def evaluate_with_openai(game: dict, api_key: str, model: str = "gpt-4") -> dict:
    """Evaluate game using OpenAI GPT."""
    if not HAS_OPENAI:
        raise ImportError("openai package not installed. Run: pip install openai")

    client = openai.OpenAI(api_key=api_key)

    transcript = format_game_transcript(game)
    winner = game.get('winner', 'unknown')

    prompt = EVALUATION_PROMPT.format(
        game_transcript=transcript,
        winner=winner
    )

    response = client.chat.completions.create(
        model=model,
        messages=[{"role": "user", "content": prompt}],
        max_tokens=4096
    )

    response_text = response.choices[0].message.content

    try:
        if "```json" in response_text:
            json_str = response_text.split("```json")[1].split("```")[0]
        elif "```" in response_text:
            json_str = response_text.split("```")[1].split("```")[0]
        else:
            json_str = response_text

        return json.loads(json_str)
    except json.JSONDecodeError:
        return {"raw_response": response_text, "parse_error": True}


def print_evaluation(evaluation: dict):
    """Pretty-print the evaluation results."""
    if evaluation.get("parse_error"):
        print("Failed to parse LLM response as JSON.")
        print("Raw response:")
        print(evaluation.get("raw_response", ""))
        return

    print("\n" + "=" * 60)
    print("GAME EVALUATION")
    print("=" * 60)

    print(f"\nOverall Rating: {evaluation.get('overall_rating', 'N/A')}/10")
    print(f"X Player Rating: {evaluation.get('x_rating', 'N/A')}/10")
    print(f"O Player Rating: {evaluation.get('o_rating', 'N/A')}/10")

    if evaluation.get('blunders'):
        print(f"\nBlunders Found: {len(evaluation['blunders'])}")
        for blunder in evaluation['blunders']:
            print(f"  - Move {blunder.get('move')}: {blunder.get('player')} at "
                  f"{blunder.get('position')} (rating: {blunder.get('rating')})")
            print(f"    {blunder.get('explanation', '')}")

    if evaluation.get('critical_moments'):
        print("\nCritical Moments:")
        for moment in evaluation['critical_moments']:
            print(f"  - Move {moment.get('move')}: {moment.get('description')}")

    if evaluation.get('suggestions'):
        print(f"\nSuggestions: {evaluation['suggestions']}")

    # Calculate average move rating
    if evaluation.get('moves'):
        ratings = [m.get('rating', 0) for m in evaluation['moves'] if m.get('rating')]
        if ratings:
            avg_rating = sum(ratings) / len(ratings)
            print(f"\nAverage Move Rating: {avg_rating:.1f}/10")

    print("\n" + "=" * 60)


def main():
    parser = argparse.ArgumentParser(description="Evaluate Gomoku games using LLM")
    parser.add_argument("--game", "-g", required=True, help="Path to game JSON file")
    parser.add_argument("--api-key", "-k", help="API key (or set ANTHROPIC_API_KEY/OPENAI_API_KEY env var)")
    parser.add_argument("--model", "-m", default="claude-sonnet-4-20250514",
                        help="Model to use (default: claude-sonnet-4-20250514)")
    parser.add_argument("--provider", "-p", choices=["anthropic", "openai"], default="anthropic",
                        help="API provider (default: anthropic)")
    parser.add_argument("--output", "-o", help="Save evaluation to JSON file")

    args = parser.parse_args()

    # Load game
    if not Path(args.game).exists():
        print(f"Error: Game file not found: {args.game}")
        sys.exit(1)

    game = load_game(args.game)

    # Get API key
    import os
    api_key = args.api_key
    if not api_key:
        if args.provider == "anthropic":
            api_key = os.environ.get("ANTHROPIC_API_KEY")
        else:
            api_key = os.environ.get("OPENAI_API_KEY")

    if not api_key:
        print(f"Error: No API key provided. Set {args.provider.upper()}_API_KEY or use --api-key")
        sys.exit(1)

    # Evaluate
    print(f"Evaluating game using {args.provider} ({args.model})...")

    if args.provider == "anthropic":
        evaluation = evaluate_with_anthropic(game, api_key, args.model)
    else:
        evaluation = evaluate_with_openai(game, api_key, args.model)

    # Print results
    print_evaluation(evaluation)

    # Save if requested
    if args.output:
        with open(args.output, 'w') as f:
            json.dump(evaluation, f, indent=2)
        print(f"\nEvaluation saved to: {args.output}")


if __name__ == "__main__":
    main()
