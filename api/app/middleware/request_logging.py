"""Middleware that decodes the JWT once, stashes it on request.state, and logs the request."""

import time

import jwt
from starlette.middleware.base import BaseHTTPMiddleware, RequestResponseEndpoint
from starlette.requests import Request
from starlette.responses import Response

from app.config import settings
from app.logger import get_logger
from app.session import CurrentSession

log = get_logger("gomoku.http")


class RequestLoggingMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next: RequestResponseEndpoint) -> Response:
        start = time.monotonic()

        method = request.method
        uri = request.url.path
        client_ip = request.client.host if request.client else "-"

        # Decode JWT once, stash typed session for downstream handlers
        session = CurrentSession()
        auth = request.headers.get("authorization", "")
        if auth.startswith("Bearer "):
            try:
                payload = jwt.decode(
                    auth[7:], settings.jwt_secret, algorithms=[settings.jwt_algorithm]
                )
                session = CurrentSession(
                    user_id=payload.get("sub"),
                    username=payload.get("username"),
                    jwt_payload=payload,
                )
            except jwt.PyJWTError:
                pass
        request.state.current_session = session

        response = await call_next(request)

        latency_ms = (time.monotonic() - start) * 1000

        log.info(
            "request",
            ip=client_ip,
            user=session.username or "-",
            method=method,
            uri=uri,
            status=response.status_code,
            latency_ms=round(latency_ms, 1),
        )

        return response
