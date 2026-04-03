-- Gomoku — Cloud SQL for PostgreSQL 17
--
-- Score formula:
--   Human loses → 0
--   Human wins  → 1000 * depth + 50 * radius + f(total_human_seconds)
--
-- f(x) rewards fast wins and penalizes slow ones:
--   x ≤ 120:  99.77 * (exp(-0.02 * (x - 120)) - 1)
--   x > 120: -28.11 * ln(1 + 0.071 * (x - 120))

-- =========================================================================
-- USERS
-- =========================================================================

CREATE TABLE IF NOT EXISTS users (
    id            UUID        DEFAULT gen_random_uuid() PRIMARY KEY,
    username      TEXT        NOT NULL UNIQUE,
    email         TEXT        UNIQUE,
    password_hash TEXT        NOT NULL,
    games_started BIGINT      NOT NULL DEFAULT 0,
    games_finished BIGINT     NOT NULL DEFAULT 0,
    created_at    TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at    TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_users_username ON users (lower(username));
CREATE UNIQUE INDEX IF NOT EXISTS idx_users_email    ON users (lower(email)) WHERE email IS NOT NULL;

CREATE TABLE IF NOT EXISTS password_reset_tokens (
    id         UUID        DEFAULT gen_random_uuid() PRIMARY KEY,
    user_id    UUID        NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    token      TEXT        NOT NULL UNIQUE,
    expires_at TIMESTAMPTZ NOT NULL,
    used       BOOLEAN     NOT NULL DEFAULT false,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_prt_token ON password_reset_tokens (token) WHERE NOT used;

-- =========================================================================
-- GAMES
-- =========================================================================

CREATE TABLE IF NOT EXISTS games (
    id            UUID        DEFAULT gen_random_uuid() PRIMARY KEY,
    player_name   TEXT        NOT NULL,
    user_id       UUID        REFERENCES users(id),
    winner        TEXT        NOT NULL CHECK (winner IN ('X', 'O', 'draw')),
    human_player  TEXT        NOT NULL CHECK (human_player IN ('X', 'O')),
    board_size    INT         NOT NULL CHECK (board_size IN (15, 19)),
    depth         INT         NOT NULL CHECK (depth BETWEEN 1 AND 10),
    radius        INT         NOT NULL CHECK (radius BETWEEN 1 AND 5),
    total_moves   INT         NOT NULL CHECK (total_moves > 0),
    human_time_s  DOUBLE PRECISION NOT NULL,
    ai_time_s     DOUBLE PRECISION NOT NULL,
    score         INT         NOT NULL DEFAULT 0,
    game_json     JSONB       NOT NULL,
    client_ip     INET,
    geo_country   TEXT,
    geo_region    TEXT,
    geo_city      TEXT,
    geo_loc       POINT,       -- (longitude, latitude)
    played_at     TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX idx_games_player   ON games (player_name);
CREATE INDEX idx_games_played   ON games (played_at DESC);
CREATE INDEX idx_games_score    ON games (score DESC);
CREATE INDEX idx_games_ip       ON games (client_ip);
CREATE INDEX idx_games_user     ON games (user_id);

-- Resolve geolocation for rows missing it (called periodically or via trigger).
CREATE OR REPLACE VIEW games_pending_geo AS
SELECT id, client_ip
FROM games
WHERE client_ip IS NOT NULL
  AND geo_country IS NULL;

-- Time bonus/penalty function
CREATE OR REPLACE FUNCTION game_time_score(seconds DOUBLE PRECISION)
RETURNS DOUBLE PRECISION
LANGUAGE sql IMMUTABLE PARALLEL SAFE AS $$
    SELECT CASE
        WHEN seconds <= 120 THEN 99.77 * (exp(-0.02 * (seconds - 120)) - 1)
        ELSE -28.11 * ln(1.0 + 0.071 * (seconds - 120))
    END
$$;

-- Full score calculation
CREATE OR REPLACE FUNCTION game_score(
    human_won BOOLEAN,
    depth     INT,
    radius    INT,
    human_seconds DOUBLE PRECISION
) RETURNS INT
LANGUAGE sql IMMUTABLE PARALLEL SAFE AS $$
    SELECT CASE
        WHEN NOT human_won THEN 0
        ELSE GREATEST(0, (1000 * depth + 50 * radius + game_time_score(human_seconds))::INT)
    END
$$;

-- Leaderboard view: best score per player
CREATE OR REPLACE VIEW leaderboard AS
SELECT DISTINCT ON (player_name)
    player_name,
    score,
    depth,
    radius,
    total_moves,
    human_time_s,
    geo_country,
    geo_city,
    played_at
FROM games
WHERE score > 0
ORDER BY player_name, score DESC;

-- Top scores view (for the global leaderboard)
CREATE OR REPLACE VIEW top_scores AS
SELECT
    player_name,
    score,
    round(score * 100.0 / 7250, 1) AS rating,  -- normalized 0-100
    depth,
    radius,
    total_moves,
    round(human_time_s::numeric, 1) AS human_time_s,
    geo_country,
    geo_city,
    played_at
FROM games
WHERE score > 0
ORDER BY score DESC
LIMIT 100;
