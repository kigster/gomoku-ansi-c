from app.scoring import game_score, rating, time_bonus


def test_time_bonus_instant_win():
    """Instant win should give ~1000 bonus."""
    bonus = time_bonus(0)
    assert 990 < bonus < 1010


def test_time_bonus_two_minutes():
    """At exactly 120 seconds, bonus should be ~0."""
    bonus = time_bonus(120)
    assert -1 < bonus < 1


def test_time_bonus_slow_game():
    """Slow games get a penalty (negative)."""
    bonus = time_bonus(300)
    assert bonus < -50


def test_time_bonus_very_slow():
    """Very slow games cap around -100."""
    bonus = time_bonus(600)
    assert -110 < bonus < -90


def test_game_score_human_loses():
    assert game_score(False, 5, 3, 60.0) == 0


def test_game_score_human_wins_depth1():
    score = game_score(True, 1, 1, 120.0)
    # 1000 * 1 + 50 * 1 + 0 = 1050
    assert score == 1050


def test_game_score_human_wins_depth5():
    score = game_score(True, 5, 3, 120.0)
    # 1000 * 5 + 50 * 3 + 0 = 5150
    assert score == 5150


def test_game_score_fast_win_bonus():
    fast = game_score(True, 3, 2, 30.0)
    slow = game_score(True, 3, 2, 300.0)
    assert fast > slow


def test_game_score_depth_dominates_speed():
    """Depth 5 slow should beat depth 1 fast."""
    depth5_slow = game_score(True, 5, 3, 300.0)
    depth1_fast = game_score(True, 1, 1, 10.0)
    assert depth5_slow > depth1_fast


def test_game_score_never_negative():
    assert game_score(True, 1, 1, 99999.0) >= 0


def test_rating_max():
    r = rating(7250)
    assert r == 100.0


def test_rating_zero():
    r = rating(0)
    assert r == 0.0


def test_rating_midrange():
    r = rating(3625)
    assert r == 50.0
