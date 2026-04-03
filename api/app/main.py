from contextlib import asynccontextmanager

import httpx
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from app.config import settings
from app.database import close_pool, create_pool
from app.middleware.client_ip import ClientIPMiddleware
from app.routers import auth, game, leaderboard, user


@asynccontextmanager
async def lifespan(fastapi_app: FastAPI):
    fastapi_app.state.db_pool = await create_pool()
    fastapi_app.state.httpx_client = httpx.AsyncClient(
        base_url=settings.gomoku_httpd_url,
        timeout=httpx.Timeout(connect=5.0, read=600.0, write=5.0, pool=5.0),
        limits=httpx.Limits(max_connections=100, max_keepalive_connections=20),
    )
    yield
    await fastapi_app.state.httpx_client.aclose()
    await close_pool()


fastapi_app = FastAPI(title="Gomoku API", version="0.1.0", lifespan=lifespan)

fastapi_app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.cors_origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

fastapi_app.include_router(auth.router)
fastapi_app.include_router(game.router)
fastapi_app.include_router(leaderboard.router)
fastapi_app.include_router(user.router)


@fastapi_app.get("/health")
async def health():
    return {"status": "ok"}


# Wrap with pure ASGI middleware (after all routes are registered)
app = ClientIPMiddleware(fastapi_app)
