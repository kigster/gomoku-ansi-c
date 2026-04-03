from starlette.types import ASGIApp, Receive, Scope, Send


class ClientIPMiddleware:
    """Pure ASGI middleware that extracts the client IP from X-Forwarded-For."""

    def __init__(self, app: ASGIApp) -> None:
        self.app = app

    async def __call__(self, scope: Scope, receive: Receive, send: Send) -> None:
        if scope["type"] in ("http", "websocket"):
            headers = dict(scope.get("headers", []))
            forwarded = headers.get(b"x-forwarded-for", b"").decode()
            if forwarded:
                scope.setdefault("state", {})["client_ip"] = forwarded.split(",")[0].strip()
            elif scope.get("client"):
                scope.setdefault("state", {})["client_ip"] = scope["client"][0]
            else:
                scope.setdefault("state", {})["client_ip"] = None
        await self.app(scope, receive, send)
