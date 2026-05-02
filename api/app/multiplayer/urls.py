"""Shared URL helpers for multiplayer-game artefacts.

Both the multiplayer router (POST /multiplayer/new → returns the
invite_url in the view) and the chat router (POST /chat/invite →
returns the invite_url in the response body) need to construct the
same URL. Centralising here keeps the scheme/domain logic in one
place — change the URL shape once and both call sites pick it up.
"""

from __future__ import annotations

from app.config import settings


def game_invite_url(code: str) -> str:
    """Public URL the host shares with the guest, e.g.
    `https://app.gomoku.games/play/AB7K3X`.

    Uses `settings.effective_domain`, which prefers `$CUSTOM_DOMAIN`
    when set (typical for local dev / staging / non-default tenants)
    and falls back to `$PUBLIC_DOMAIN` otherwise. The scheme is `http`
    only when the domain looks loopback, so production deployments
    always speak HTTPS without explicit configuration.
    """
    domain = settings.effective_domain
    scheme = (
        "http"
        if domain.startswith("localhost") or domain.startswith("127.")
        else "https"
    )
    return f"{scheme}://{domain}/play/{code}"
