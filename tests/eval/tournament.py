#!/usr/bin/env python3
import argparse
import subprocess
import json
import os
import time
import sys
import tempfile
import statistics

def run_game(gomoku_path, depth1, depth2, board_size, radius):
    """
    Runs a single game of gomoku between two AI players.
    Returns a dictionary with game statistics.
    """
    # Create a temporary file for JSON output
    fd, json_path = tempfile.mkstemp(suffix='.json')
    os.close(fd)

    cmd = [
        gomoku_path,
        "-x", "ai",
        "-o", "ai",
        "-d", f"{depth1}:{depth2}",
        "-b", str(board_size),
        "-r", str(radius),
        "--skip-welcome",
        "-j", json_path
    ]

    start_time = time.time()
    
    try:
        # Run process and capture output to prevent it from cluttering the terminal
        result = subprocess.run(
            cmd, 
            capture_output=True, 
            text=True, 
            input="q\n", # Send 'q' to quit at end of game
            timeout=300 # 5 minute timeout per game
        )
        
        duration = time.time() - start_time
        
        if result.returncode != 0:
            print(f"Error running game: {result.stderr}")
            if os.path.exists(json_path):
                os.remove(json_path)
            return None

        # Parse the JSON result
        if not os.path.exists(json_path):
             print(f"Error: JSON file not created. Stderr: {result.stderr}")
             return None

        with open(json_path, 'r') as f:
            try:
                game_data = json.load(f)
            except json.JSONDecodeError:
                print(f"Error: Invalid JSON output. Stderr: {result.stderr}")
                os.remove(json_path)
                return None
            
        return {
            "winner": game_data.get("winner"), # "X", "O", or "draw"
            "moves": len(game_data.get("moves", [])),
            "duration": duration,
            "depth_x": depth1,
            "depth_o": depth2
        }
        
    except subprocess.TimeoutExpired:
        print("Game timed out!")
        return {"winner": "timeout", "duration": 300}
    except Exception as e:
        print(f"Exception: {e}")
        return None
    finally:
        # Clean up temporary file
        if os.path.exists(json_path):
            os.remove(json_path)

def main():
    parser = argparse.ArgumentParser(description="Run Gomoku AI Tournament")
    parser.add_argument("--bin", default="./gomoku", help="Path to gomoku binary")
    parser.add_argument("--games", type=int, default=10, help="Number of games to play")
    parser.add_argument("--depth1", type=int, default=2, help="Depth for Player X")
    parser.add_argument("--depth2", type=int, default=2, help="Depth for Player O")
    parser.add_argument("--board", type=int, default=15, help="Board size (15 or 19)")
    parser.add_argument("--radius", type=int, default=3, help="Search radius")
    
    args = parser.parse_args()
    
    if not os.path.exists(args.bin):
        print(f"Error: Binary not found at {args.bin}")
        sys.exit(1)

    print(f"Starting Tournament: {args.games} games")
    print(f"Player X (Depth {args.depth1}) vs Player O (Depth {args.depth2})")
    print(f"Board Size: {args.board}, Radius: {args.radius}")
    print("-" * 60)
    print(f"{'Game':<5} | {'Winner':<8} | {'Moves':<6} | {'Time (s)':<8}")
    print("-" * 60)

    stats = {
        "X": 0,
        "O": 0,
        "draw": 0,
        "timeout": 0,
        "total_moves": [],
        "total_time": []
    }

    for i in range(args.games):
        res = run_game(args.bin, args.depth1, args.depth2, args.board, args.radius)
        
        if res:
            winner = res.get("winner", "unknown")
            
            if winner in ["X", "O", "draw"]:
                stats[winner] += 1
            else:
                # Count as timeout or error if not standard winner
                if winner == "timeout":
                    stats["timeout"] += 1
                else: 
                     # Should not happen typically
                     pass

            stats["total_moves"].append(res.get("moves", 0))
            stats["total_time"].append(res.get("duration", 0))

            print(f"{i+1:<5} | {winner:<8} | {res.get('moves', 0):<6} | {res.get('duration', 0):<8.2f}")
        else:
            print(f"{i+1:<5} | ERROR    | 0      | 0.00")

    print("-" * 60)
    print("Results Summary:")
    print(f"Player X (Depth {args.depth1}) Wins: {stats['X']} ({stats['X']/args.games*100:.1f}%)")
    print(f"Player O (Depth {args.depth2}) Wins: {stats['O']} ({stats['O']/args.games*100:.1f}%)")
    print(f"Draws: {stats['draw']}")
    print(f"Timeouts: {stats['timeout']}")
    
    if stats['total_moves']:
        avg_moves = statistics.mean(stats['total_moves'])
        avg_time = statistics.mean(stats['total_time'])
        print(f"Avg Moves per Game: {avg_moves:.1f}")
        print(f"Avg Time per Game:  {avg_time:.2f}s")

if __name__ == "__main__":
    main()
