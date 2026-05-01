import json as json_mod
import time

from fastapi import APIRouter, Depends, HTTPException, Query, Request, status
from fastapi.responses import JSONResponse
from httpx import AsyncClient

from app.database import get_pool
from app.elo import ai_tier_rating, k_factor
from app.elo import update as elo_update
from app.logger import get_logger
from app.models.game import GameHistoryEntry, GameHistoryResponse, GameSaveRequest, GameSaveResponse
from app.scoring import game_score, rating
from app.security import get_current_user

router = APIRouter(prefix="/game", tags=["game"])
log = get_logger("gomoku.game")


def log_game_request(
    request: Request, start_time: float, status_code: int, payload_bytes: int
) -> None:
    latency_ms = (time.monotonic() - start_time) * 1000
    log.info(
        "game_route",
        method=request.method,
        path=request.url.path,
        status=status_code,
        payload_bytes=payload_bytes,
        latency_ms=round(latency_ms, 1),
    )


@router.post("/play")
async def play(request: Request):
    """Proxy game state to gomoku-httpd and return the AI's response."""
    start_time = time.monotonic()
    body = await request.body()
    client: AsyncClient = request.app.state.httpx_client
    try:
        resp = await client.post(
            "/gomoku/play",
            content=body,
            headers={"Content-Type": "application/json"},
        )
    except Exception as exc:
        log.error("engine_request_failed", error=str(exc), error_type=type(exc).__name__)
        log_game_request(request, start_time, status.HTTP_503_SERVICE_UNAVAILABLE, len(body))
        raise HTTPException(
            status.HTTP_503_SERVICE_UNAVAILABLE,
            "Game engine unavailable, please retry",
        )
    log_game_request(request, start_time, resp.status_code, len(body))

    if resp.status_code != 200:
        # Surface the engine's actual response so we can debug auth / routing
        # issues without the JSON-decode crash that previously masked the cause.
        log.error(
            "engine_non_200",
            status=resp.status_code,
            content_type=resp.headers.get("content-type", "(none)"),
            body_preview=resp.text[:500] if resp.text else "(empty)",
        )
        raise HTTPException(
            status.HTTP_502_BAD_GATEWAY,
            f"Game engine returned HTTP {resp.status_code}",
        )

    return resp.json()


@router.post("/start")
async def start(
    request: Request,
    user: dict = Depends(get_current_user),
    pool=Depends(get_pool),
):
    """Record that a user started a game."""
    start_time = time.monotonic()
    body = await request.body()
    await pool.execute(
        "UPDATE users SET games_started = games_started + 1 WHERE id = $1::uuid",
        str(user["id"]),
    )
    log_game_request(request, start_time, status.HTTP_200_OK, len(body))
    return {"status": "ok"}


@router.post("/save", response_model=GameSaveResponse)
async def save(
    body: GameSaveRequest,
    request: Request,
    user: dict = Depends(get_current_user),
    pool=Depends(get_pool),
):
    """Save a completed game with score calculation."""
    start_time = time.monotonic()
    raw_body = await request.body()
    gj = body.game_json
    winner = gj.get("winner", "none")
    if winner == "none":
        log_game_request(request, start_time, status.HTTP_400_BAD_REQUEST, len(raw_body))
        raise HTTPException(status.HTTP_400_BAD_REQUEST, "Game is not finished")

    # Determine which side is human
    x_conf = gj.get("X", {})
    o_conf = gj.get("O", {})
    if x_conf.get("player") == "human":
        human_player = "X"
        ai_depth = o_conf.get("depth", 3)
    elif o_conf.get("player") == "human":
        human_player = "O"
        ai_depth = x_conf.get("depth", 3)
    else:
        human_player = "X"
        ai_depth = max(x_conf.get("depth", 3), o_conf.get("depth", 3))

    moves = gj.get("moves", [])
    human_time_s = 0.0
    ai_time_s = 0.0
    for m in moves:
        ms = m.get("time_ms", 0)
        secs = ms / 1000.0
        if f"{human_player} (human)" in m:
            human_time_s += secs
        else:
            ai_time_s += secs

    human_won = winner == human_player
    radius = gj.get("radius", 2)
    score = game_score(human_won, ai_depth, radius, human_time_s)

    client_ip = getattr(request.state, "client_ip", None) if hasattr(request, "state") else None
    user_id = str(user["id"])

    # Elo update against the AI tier the human chose. Draws are rare in
    # Gomoku (eloDraw=0.01 in BayesElo), but we treat anything that isn't
    # a clear human win as a loss for now — the engine never resigns and
    # the C side never reports 'draw' yet.
    score_a = 1.0 if human_won else 0.0
    opponent_rating = ai_tier_rating(ai_depth, radius)

    async with pool.acquire() as conn:
        async with conn.transaction():
            user_row = await conn.fetchrow(
                "SELECT elo_rating, elo_peak, elo_games_count FROM users WHERE id = $1::uuid",
                user_id,
            )
            elo_before = int(user_row["elo_rating"])
            games_before = int(user_row["elo_games_count"])
            peak_before = int(user_row["elo_peak"])
            k = k_factor(games_before, elo_before)
            elo_after = elo_update(elo_before, opponent_rating, score_a, k)
            peak_after = max(peak_before, elo_after)

            row = await conn.fetchrow(
                """INSERT INTO games
                   (username, user_id, winner, human_player, board_size, depth, radius,
                    total_moves, human_time_s, ai_time_s, score, game_json, client_ip,
                    elo_before, elo_after, opponent_elo_before)
                   VALUES ($1, $2::uuid, $3, $4, $5, $6, $7,
                           $8, $9, $10, $11, $12::jsonb, $13::inet,
                           $14, $15, $16)
                   RETURNING id""",
                user["username"],
                user_id,
                winner,
                human_player,
                gj.get("board_size", 19),
                ai_depth,
                radius,
                len(moves),
                human_time_s,
                ai_time_s,
                score,
                json_mod.dumps(gj),
                client_ip,
                elo_before,
                elo_after,
                opponent_rating,
            )
            await conn.execute(
                """UPDATE users
                       SET games_finished = games_finished + 1,
                           elo_rating = $2,
                           elo_peak = $3,
                           elo_games_count = elo_games_count + 1,
                           updated_at = now()
                     WHERE id = $1::uuid""",
                user_id,
                elo_after,
                peak_after,
            )

    log_game_request(request, start_time, status.HTTP_200_OK, len(raw_body))
    return GameSaveResponse(
        id=str(row["id"]),
        score=score,
        rating=rating(score),
        elo_before=elo_before,
        elo_after=elo_after,
        elo_delta=elo_after - elo_before,
    )


@router.get("/history", response_model=GameHistoryResponse)
async def game_history(
    request: Request,
    limit: int = Query(default=50, ge=1, le=200),
    user: dict = Depends(get_current_user),
    pool=Depends(get_pool),
):
    """Return the authenticated user's games in reverse chronological order."""
    start_time = time.monotonic()
    rows = await pool.fetch(
        """SELECT g.id, g.username, g.winner, g.human_player, g.score, g.depth,
                  round(g.human_time_s::numeric, 1) AS human_time_s,
                  round(g.ai_time_s::numeric, 1) AS ai_time_s,
                  g.played_at, g.game_type,
                  g.elo_before, g.elo_after, g.opponent_elo_before,
                  opp.username AS opponent_username
           FROM games g
           LEFT JOIN users opp ON opp.id = g.opponent_id
           WHERE g.user_id = $1::uuid
           ORDER BY g.played_at DESC
           LIMIT $2""",
        str(user["id"]),
        limit,
    )
    response = GameHistoryResponse(
        games=[
            GameHistoryEntry(
                id=str(r["id"]),
                username=r["username"],
                won=r["winner"] == r["human_player"],
                score=r["score"],
                depth=r["depth"],
                human_time_s=float(r["human_time_s"]),
                ai_time_s=float(r["ai_time_s"]),
                played_at=r["played_at"],
                game_type=r["game_type"],
                opponent_username=r["opponent_username"] or "AI",
                elo_before=r["elo_before"],
                elo_after=r["elo_after"],
                opponent_elo_before=r["opponent_elo_before"],
            )
            for r in rows
        ]
    )
    log_game_request(request, start_time, status.HTTP_200_OK, 0)
    return response


@router.get("/{game_id}/json")
async def download_game_json(
    request: Request,
    game_id: str,
    user: dict = Depends(get_current_user),
    pool=Depends(get_pool),
):
    """Return the full game JSON for a single game (for download)."""
    start_time = time.monotonic()
    row = await pool.fetchrow(
        "SELECT game_json FROM games WHERE id = $1::uuid AND user_id = $2::uuid",
        game_id,
        str(user["id"]),
    )
    if row is None:
        log_game_request(request, start_time, status.HTTP_404_NOT_FOUND, 0)
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Game not found")
    data = row["game_json"]
    if isinstance(data, str):
        data = json_mod.loads(data)
    log_game_request(request, start_time, status.HTTP_200_OK, 0)
    return JSONResponse(content=data)
