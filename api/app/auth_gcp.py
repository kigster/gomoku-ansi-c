"""GCP service-to-service auth for httpx.

Cloud Run services with ``ingress = INGRESS_TRAFFIC_INTERNAL_ONLY`` (the
engine, in our case) require callers to present a Google-issued ID token whose
audience matches the target service's URL. The IAM binding granting
``roles/run.invoker`` to the caller's service account is necessary but not
sufficient — the caller still has to prove identity per-request.

This module provides a small ``httpx.Auth`` flow that fetches such a token via
Google's Application Default Credentials (the Cloud Run metadata server in
production, gcloud user creds locally) and caches it for ~50 minutes (Google
issues 1h tokens).

In environments without ADC (local dev, CI), token fetch fails silently and
the request goes out without an Authorization header — the local engine
container has no auth, and an unauthenticated request is the right behaviour.
"""

from __future__ import annotations

import time

import httpx

from app.logger import get_logger

log = get_logger("gomoku.gcp_auth")

try:
    import google.auth.transport.requests as _g_transport
    from google.oauth2 import id_token as _g_id_token

    _GOOGLE_AVAILABLE = True
except ImportError:
    _GOOGLE_AVAILABLE = False


class GCPIdentityAuth(httpx.Auth):
    """Attach a Google ID token bound to ``audience`` on every outgoing request.

    Caches the token for 50 minutes (tokens last 1h). On any failure to fetch
    (no ADC, no metadata server, network error), proceeds without a header
    and logs a single info-level line so dev runs don't spam.
    """

    requires_request_body = False

    def __init__(self, audience: str) -> None:
        self.audience = audience
        self._token: str | None = None
        self._fetched_at: float = 0.0
        self._failure_logged = False

    def _maybe_token(self) -> str | None:
        if not _GOOGLE_AVAILABLE:
            return None
        now = time.time()
        if self._token and now - self._fetched_at < 50 * 60:
            return self._token
        try:
            req = _g_transport.Request()
            self._token = _g_id_token.fetch_id_token(req, self.audience)
            self._fetched_at = now
            return self._token
        except Exception as exc:  # broad: any failure means "fall back to no-auth"
            if not self._failure_logged:
                log.info(
                    "gcp_auth_unavailable",
                    error=str(exc),
                    audience=self.audience,
                )
                self._failure_logged = True
            return None

    def auth_flow(self, request: httpx.Request):
        token = self._maybe_token()
        if token:
            request.headers["Authorization"] = f"Bearer {token}"
        yield request

    def warm(self) -> bool:
        """Eagerly fetch a token so startup logs show whether ADC is reachable.

        Returns True on success, False otherwise. Safe to call from sync code.
        """
        token = self._maybe_token()
        if token:
            log.info(
                "gcp_auth_ready",
                audience=self.audience,
                token_preview=f"{token[:16]}...",
            )
            return True
        return False
