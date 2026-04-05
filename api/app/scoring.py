from math import exp, log

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
