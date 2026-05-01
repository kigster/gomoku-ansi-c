"""Unit tests for ``app.elo`` — pure-function tests, no DB."""

import pytest

from app.elo import (
    INITIAL_RATING,
    ai_tier_rating,
    expected,
    k_factor,
    update,
)

# ---------------------------------------------------------------------------
# expected()
# ---------------------------------------------------------------------------


def test_expected_equal_ratings_is_half() -> None:
    assert expected(1500, 1500) == pytest.approx(0.5)


def test_expected_400_higher_is_about_91_percent() -> None:
    # Standard Elo property: a 400-point advantage ~= 10/11 expected score.
    assert expected(1900, 1500) == pytest.approx(10 / 11, rel=1e-3)


def test_expected_400_lower_is_about_9_percent() -> None:
    assert expected(1500, 1900) == pytest.approx(1 / 11, rel=1e-3)


def test_expected_is_symmetric_around_half() -> None:
    # For any two ratings, exp(a,b) + exp(b,a) == 1.0
    pairs = [(1200, 1800), (1500, 1500), (2000, 1450), (800, 2400)]
    for a, b in pairs:
        assert expected(a, b) + expected(b, a) == pytest.approx(1.0)


def test_expected_is_monotone_in_difference() -> None:
    # Larger advantage = larger expectation.
    assert expected(1600, 1500) > expected(1500, 1500)
    assert expected(1700, 1500) > expected(1600, 1500)


# ---------------------------------------------------------------------------
# k_factor()
# ---------------------------------------------------------------------------


def test_k_factor_provisional() -> None:
    assert k_factor(0, 1500) == 40
    assert k_factor(19, 1500) == 40


def test_k_factor_calibrating() -> None:
    assert k_factor(20, 1500) == 24
    assert k_factor(99, 1500) == 24


def test_k_factor_stable() -> None:
    assert k_factor(100, 1500) == 16
    assert k_factor(999, 2200) == 16


def test_k_factor_top_tier_drops_to_10() -> None:
    assert k_factor(500, 2400) == 10
    assert k_factor(500, 2800) == 10


# ---------------------------------------------------------------------------
# update()
# ---------------------------------------------------------------------------


def test_update_equal_rating_win_gains_half_k() -> None:
    # expected = 0.5, win => delta = K * (1 - 0.5) = K/2
    assert update(1500, 1500, 1.0, 40) == 1520


def test_update_equal_rating_loss_loses_half_k() -> None:
    assert update(1500, 1500, 0.0, 40) == 1480


def test_update_equal_rating_draw_is_noop() -> None:
    assert update(1500, 1500, 0.5, 40) == 1500


def test_update_is_zero_sum_for_equal_K() -> None:
    # Sum of both players' deltas == 0 when K is the same.
    a_before, b_before = 1600, 1400
    a_after = update(a_before, b_before, 1.0, 16)
    b_after = update(b_before, a_before, 0.0, 16)
    assert (a_after - a_before) + (b_after - b_before) == 0


def test_update_underdog_win_swings_more_than_favourite_win() -> None:
    # Beating a higher-rated opponent should give more rating than beating
    # a lower-rated one, all else equal.
    favourite_win = update(1800, 1400, 1.0, 16) - 1800
    underdog_win = update(1400, 1800, 1.0, 16) - 1400
    assert underdog_win > favourite_win


def test_update_rejects_invalid_score() -> None:
    with pytest.raises(ValueError):
        update(1500, 1500, 1.5, 32)
    with pytest.raises(ValueError):
        update(1500, 1500, -0.1, 32)


# ---------------------------------------------------------------------------
# ai_tier_rating()
# ---------------------------------------------------------------------------


def test_ai_tier_rating_seeded() -> None:
    # Spot-check the depth-5 tier humans care about most.
    assert ai_tier_rating(5, 3) == 2050


def test_ai_tier_rating_monotone_in_depth() -> None:
    # Same radius, increasing depth should increase rating.
    for r in (2, 3):
        ratings = [ai_tier_rating(d, r) for d in (2, 3, 4, 5)]
        assert ratings == sorted(ratings) and len(set(ratings)) == 4


def test_ai_tier_rating_monotone_in_radius() -> None:
    for d in (3, 4, 5):
        ratings = [ai_tier_rating(d, r) for r in (1, 2, 3, 4)]
        # Strictly increasing for r=1..3, non-decreasing into r=4.
        assert ratings[0] < ratings[1] < ratings[2] <= ratings[3]


def test_ai_tier_rating_fallback_for_unknown_tier() -> None:
    # Depth 8, radius 5 isn't seeded; the fallback must still produce a
    # sane positive integer.
    fallback = ai_tier_rating(8, 5)
    assert fallback > ai_tier_rating(5, 3)


# ---------------------------------------------------------------------------
# Module-level constants
# ---------------------------------------------------------------------------


def test_initial_rating_is_chess_standard() -> None:
    assert INITIAL_RATING == 1500
