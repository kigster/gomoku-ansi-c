# Gomoku AI Evaluation Tools

This directory contains tools for evaluating the performance of the Gomoku AI and testing the `gomoku-httpd` server.

## Contents

- `tournament.py`: Automated CLI tournament runner.
- `llm_player.py`: Interaction script for LLMs or human vs Server.
- `depth_tournament.sh`: Legacy shell script for tournaments (deprecated).
- `results/`: Directory for storing tournament results.

## Tournament Runner (`tournament.py`)

Run automated matches between different AI difficulty levels (depths) to generate win-rate statistics.

### Usage

```bash
python3 tournament.py --games 10 --depth1 2 --depth2 4 --board 15
```

**Options:**
- `--bin`: Path to gomoku binary (default: `./gomoku` in root)
- `--games`: Number of games to play
- `--depth1`: Search depth for Player X
- `--depth2`: Search depth for Player O
- `--board`: Board size (15 or 19)
- `--radius`: Search radius

## LLM Player (`llm_player.py`)

A client to play against the `gomoku-httpd` server. Can be used manually or with an OpenAI-compatible API.

### Usage

**Manual Play:**
```bash
python3 llm_player.py --manual --url http://127.0.0.1:8080
```

**LLM Play:**
```bash
export OPENAI_API_KEY="sk-..."
python3 llm_player.py --model gpt-4 --url http://127.0.0.1:8080
```

**Options:**
- `--url`: URL of the gomoku-httpd server
- `--model`: Model name (default: gpt-4)
- `--manual`: Enable manual input mode
- `--ai-starts`: AI plays X (starts)
- `--size`: Board size
