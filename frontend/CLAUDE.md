# Gomoku Front-End

React + TypeScript + Vite + TailwindCSS single-page application for the Gomoku game.

## Architecture

The frontend talks exclusively to the **FastAPI server** (`api/`). It has no
direct connection to `gomoku-httpd`. The API call flow is:

```
React App → FastAPI → gomoku-httpd (for AI moves)
               ↓
           PostgreSQL (for auth, scores, leaderboard)
```

### API Endpoints Used

| Path | Purpose |
|---|---|
| `POST /auth/signup` | Create account |
| `POST /auth/login` | Login, returns JWT |
| `POST /auth/password-reset` | Request password reset email |
| `POST /auth/password-reset/confirm` | Set new password with token |
| `POST /game/play` | Send game state, get AI move back |
| `POST /game/start` | Record that user started a game |
| `POST /game/save` | Save completed game with score |
| `GET /leaderboard` | Top 100 global scores |
| `GET /user/me` | Current user profile + personal best |
| `GET /health` | Health check |

### Environment Variables

| Variable | Default | Purpose |
|---|---|---|
| `VITE_API_BASE` | `""` (same-origin) | API base URL. Leave empty for dev (vite proxy) and production (nginx proxy). |

## Development

```bash
npm install
npm run dev          # Vite dev server on :5173, proxies API to :8000
npm run build        # Production build to dist/
npx vitest run       # Run tests
npx tsc --noEmit     # Type check
```

Requires FastAPI running on port 8000 (`cd ../api && just serve`).

## Production (Docker)

The Docker image serves the built React app via nginx and proxies all API
routes to the FastAPI backend (configured via `API_URL` env var).

```bash
docker build -t gomoku-frontend:latest .
docker run -p 80:80 -e API_URL=http://gomoku-api:8000 gomoku-frontend:latest
```

## Key Components

- **AuthModal** — Login/Signup tabs, forgot password, reset password (from email link)
- **AlertPanel** — Centered notifications with fade-in/out, three types (error/info/warning), expandable error details
- **LeaderboardModal** — Top 100 global players with scores and geo
- **Board** — SVG-based 19x19 (or 15x15) game board with stone/XO display modes
- **GameStatus** — Move counter, timers, player info, error display
- **SettingsPanel** — AI depth, radius, timeout, display mode, side selection

## Auth Flow

1. First visit → AuthModal (Sign Up tab)
2. User creates account → JWT stored in localStorage
3. Subsequent visits → JWT read from localStorage, game UI loads
4. `?token=...` in URL → password reset view
5. Log Out → clears localStorage, shows AuthModal

## Game Flow

1. User clicks Start Game → `POST /game/start` (increment counter)
2. Human places stone → `POST /game/play` with full game JSON
3. FastAPI proxies to gomoku-httpd → AI move returned
4. Repeat until win/draw
5. Game over → `POST /game/save` with game JSON → score calculated and stored
6. Green/red alert shows result and score
