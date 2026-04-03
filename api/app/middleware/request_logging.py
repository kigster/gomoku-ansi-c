"""Middleware that logs every request/response, including validation error bodies."""

import logging
import time

from starlette.middleware.base import BaseHTTPMiddleware, RequestResponseEndpoint
from starlette.requests import Request
from starlette.responses import Response

logger = logging.getLogger("gomoku.http")

# Max bytes of request body to log on error
MAX_BODY_LOG = 2048


class RequestLoggingMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next: RequestResponseEndpoint) -> Response:
        start = time.monotonic()
        method = request.method
        path = request.url.path
        client_ip = request.client.host if request.client else "unknown"

        # Cache the body so we can log it if the response is a 4xx/5xx
        body = b""
        if method in ("POST", "PUT", "PATCH"):
            body = await request.body()

        logger.debug("%s %s from %s", method, path, client_ip)

        response = await call_next(request)

        elapsed_ms = (time.monotonic() - start) * 1000
        status = response.status_code

        if status >= 400:
            log_fn = logger.warning if status < 500 else logger.error
            content_type = request.headers.get("content-type", "")
            body_preview = body[:MAX_BODY_LOG].decode("utf-8", errors="replace") if body else ""
            log_fn(
                "%s %s → %d (%.1fms) body=[%s] content-type=%s",
                method,
                path,
                status,
                elapsed_ms,
                body_preview,
                content_type,
            )
        else:
            logger.info("%s %s → %d (%.1fms)", method, path, status, elapsed_ms)

        return response
