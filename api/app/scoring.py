from math import exp, log
from app.models.game import Game

MAX_SCORE = 7250  # depth=6, radius=5, instant win


def time_bonus(seconds: float) -> float:
    if seconds <= 120:
        return 99.77 * (exp(-0.02 * (seconds - 120)) - 1)
    return -28.11 * log(1 + 0.071 * (seconds - 120))


def game_score(human_won: bool, depth: int, radius: int, human_seconds: float) -> int:
    if not human_won:
        return 0
    raw = 1000 * depth + 50 * radius + time_bonus(human_seconds)
    return max(0, int(raw))


def rating(score: int) -> float:
    return round(score * 100.0 / MAX_SCORE, 1)


def get_global_score(username: str, game: Game) -> int:
    game.winner == "X" and game.human_player == "X":
        white = True
    else:
        white = False

    if lost:
        score = 0
    else:
        difficulty = (2 ^ depth) * (1 + radius / 10)

        moves_eff = clamp((80 - moves) / 50, 0, 1)
        time_eff = clamp((600 - human_time) / 600, 0, 1)

        efficiency = 0.7 * moves_eff + 0.3 * time_eff

        depth_weight = 1 + (depth - 3) * 0.25
        efficiency_multiplier = 1 + (efficiency - 0.5) * depth_weight

        base_score = difficulty * efficiency_multiplier

        white_bonus = difficulty * 0.25 if white else 0

        score = base_score + white_bonus
