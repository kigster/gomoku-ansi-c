import json as json_mod

from fastapi import APIRouter, Depends, HTTPException, Query, Request, status
from httpx import AsyncClient

from app.database import get_pool
from app.models.game import GameHistoryEntry, GameHistoryResponse, GameSaveRequest, GameSaveResponse
from app.scoring import game_score, rating
from app.security import get_current_user

router = APIRouter(prefix="/game", tags=["game"])


@router.post("/play")
async def play(request: Request):
    """Proxy game state to gomoku-httpd and return the AI's response."""
    body = await request.body()
    client: AsyncClient = request.app.state.httpx_client
    try:
        resp = await client.post(
            "/gomoku/play", content=body,
            headers={"Content-Type": "application/json"},
        )
    except Exception:
        raise HTTPException(
            status.HTTP_503_SERVICE_UNAVAILABLE,
            "Game engine unavailable, please retry",
        )
    return resp.json()


@router.post("/start")
async def start(
    user: dict = Depends(get_current_user),
    pool=Depends(get_pool),
):
    """Record that a user started a game."""
    await pool.execute(
        "UPDATE users SET games_started = games_started + 1 WHERE id = $1::uuid",
        str(user["id"]),
    )
    return {"status": "ok"}


@router.post("/save", response_model=GameSaveResponse)
async def save(
    body: GameSaveRequest,
    request: Request,
    user: dict = Depends(get_current_user),
    pool=Depends(get_pool),
):
    """Save a completed game with score calculation."""
    gj = body.game_json
    winner = gj.get("winner", "none")
    if winner == "none":
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

    async with pool.acquire() as conn:
        async with conn.transaction():
            row = await conn.fetchrow(
                """INSERT INTO games
                   (player_name, user_id, winner, human_player, board_size, depth, radius,
                    total_moves, human_time_s, ai_time_s, score, game_json, client_ip)
                   VALUES ($1, $2::uuid, $3, $4, $5, $6, $7,
                           $8, $9, $10, $11, $12::jsonb, $13::inet)
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
            )
            await conn.execute(
                "UPDATE users SET games_finished = games_finished + 1 WHERE id = $1::uuid",
                user_id,
            )

    return GameSaveResponse(id=str(row["id"]), score=score, rating=rating(score))


@router.get("/history", response_model=GameHistoryResponse)
async def game_history(
    limit: int = Query(default=50, ge=1, le=200),
    user: dict = Depends(get_current_user),
    pool=Depends(get_pool),
):
    """Return the authenticated user's games in reverse chronological order."""
    rows = await pool.fetch(
        """SELECT id, player_name, winner, human_player, score, depth,
                  round(human_time_s::numeric, 1) AS human_time_s,
                  round(ai_time_s::numeric, 1) AS ai_time_s,
                  played_at
           FROM games
           WHERE user_id = $1::uuid
           ORDER BY played_at DESC
           LIMIT $2""",
        str(user["id"]),
        limit,
    )
    return GameHistoryResponse(
        games=[
            GameHistoryEntry(
                id=str(r["id"]),
                player_name=r["player_name"],
                won=r["winner"] == r["human_player"],
                score=r["score"],
                depth=r["depth"],
                human_time_s=float(r["human_time_s"]),
                ai_time_s=float(r["ai_time_s"]),
                played_at=r["played_at"],
            )
            for r in rows
        ]
    )


@router.get("/{game_id}/json")
async def download_game_json(
    game_id: str,
    user: dict = Depends(get_current_user),
    pool=Depends(get_pool),
):
    """Return the full game JSON for a single game (for download)."""
    row = await pool.fetchrow(
        "SELECT game_json FROM games WHERE id = $1::uuid AND user_id = $2::uuid",
        game_id,
        str(user["id"]),
    )
    if row is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Game not found")
    return row["game_json"]
