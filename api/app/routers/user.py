from fastapi import APIRouter, Depends

from app.database import get_pool
from app.models.user import PersonalBest, UserOut
from app.scoring import rating
from app.security import get_current_user

router = APIRouter(prefix="/user", tags=["user"])


@router.get("/me", response_model=UserOut)
async def get_me(user: dict = Depends(get_current_user), pool=Depends(get_pool)):
    best = await pool.fetchrow(
        """SELECT score, depth, radius, played_at
           FROM games
           WHERE user_id = $1::uuid AND score > 0
           ORDER BY score DESC
           LIMIT 1""",
        str(user["id"]),
    )
    personal_best = None
    if best:
        personal_best = PersonalBest(
            score=best["score"],
            rating=rating(best["score"]),
            depth=best["depth"],
            radius=best["radius"],
            played_at=best["played_at"],
        )

    return UserOut(
        id=str(user["id"]),
        username=user["username"],
        email=user.get("email"),
        created_at=user["created_at"],
        personal_best=personal_best,
    )
