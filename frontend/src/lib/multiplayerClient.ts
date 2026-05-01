// Typed wrappers around the /multiplayer/* API. All endpoints require the
// user's JWT, which we pass via the Authorization header (token is read
// from sessionStorage by the caller).

const API_BASE = import.meta.env.VITE_API_BASE || ''

export type GameStateName = 'waiting' | 'in_progress' | 'finished' | 'abandoned'
export type Color = 'X' | 'O'

export interface PlayerInfo {
  username: string
  color: Color
}

export interface MultiplayerGameView {
  code: string
  state: GameStateName
  board_size: number
  rule_set: string
  host: PlayerInfo
  guest: PlayerInfo | null
  moves: [number, number][]
  next_to_move: Color
  winner: Color | 'draw' | null
  your_color: Color | null
  your_turn: boolean
  version: number
  created_at: string
  finished_at: string | null
}

export interface MultiplayerGamePreview {
  code: string
  state: GameStateName
  board_size: number
  rule_set: string
  host: PlayerInfo
  guest: PlayerInfo | null
  next_to_move: Color
  winner: Color | 'draw' | null
  your_color: null
  your_turn: false
  version: number
  created_at: string
  finished_at: string | null
}

export class MultiplayerApiError extends Error {
  status: number
  detail: string

  constructor(status: number, detail: string) {
    super(`HTTP ${status}: ${detail}`)
    this.status = status
    this.detail = detail
  }
}

function authHeaders(token: string): HeadersInit {
  return {
    'Content-Type': 'application/json',
    Authorization: `Bearer ${token}`,
  }
}

async function parseError(response: Response): Promise<MultiplayerApiError> {
  let detail = ''
  try {
    const body = await response.json()
    detail = typeof body.detail === 'string' ? body.detail : JSON.stringify(body)
  } catch {
    detail = await response.text().catch(() => '')
  }
  return new MultiplayerApiError(response.status, detail)
}

export async function newGame(
  token: string,
  opts: { board_size?: 15 | 19; host_color?: Color } = {},
): Promise<MultiplayerGameView> {
  const response = await fetch(`${API_BASE}/multiplayer/new`, {
    method: 'POST',
    headers: authHeaders(token),
    body: JSON.stringify(opts),
  })
  if (!response.ok) throw await parseError(response)
  return response.json() as Promise<MultiplayerGameView>
}

export async function joinGame(
  token: string,
  code: string,
): Promise<MultiplayerGameView> {
  const response = await fetch(`${API_BASE}/multiplayer/${code}/join`, {
    method: 'POST',
    headers: authHeaders(token),
    body: JSON.stringify({}),
  })
  if (!response.ok) throw await parseError(response)
  return response.json() as Promise<MultiplayerGameView>
}

/** Returns the view, or `null` if the server returned 304 (no change since
 *  `sinceVersion`). */
export async function getGame(
  token: string,
  code: string,
  sinceVersion?: number,
): Promise<MultiplayerGameView | MultiplayerGamePreview | null> {
  const url = new URL(`${API_BASE}/multiplayer/${code}`, window.location.origin)
  if (sinceVersion !== undefined) {
    url.searchParams.set('since_version', String(sinceVersion))
  }
  const response = await fetch(url.toString(), {
    headers: { Authorization: `Bearer ${token}` },
  })
  if (response.status === 304) return null
  if (!response.ok) throw await parseError(response)
  return response.json()
}

export async function postMove(
  token: string,
  code: string,
  x: number,
  y: number,
  expectedVersion: number,
): Promise<MultiplayerGameView> {
  const response = await fetch(`${API_BASE}/multiplayer/${code}/move`, {
    method: 'POST',
    headers: authHeaders(token),
    body: JSON.stringify({ x, y, expected_version: expectedVersion }),
  })
  if (!response.ok) throw await parseError(response)
  return response.json() as Promise<MultiplayerGameView>
}

export async function resignGame(
  token: string,
  code: string,
): Promise<MultiplayerGameView> {
  const response = await fetch(`${API_BASE}/multiplayer/${code}/resign`, {
    method: 'POST',
    headers: authHeaders(token),
    body: JSON.stringify({}),
  })
  if (!response.ok) throw await parseError(response)
  return response.json() as Promise<MultiplayerGameView>
}

export async function listMyGames(token: string): Promise<MultiplayerGameView[]> {
  const response = await fetch(`${API_BASE}/multiplayer/mine`, {
    headers: { Authorization: `Bearer ${token}` },
  })
  if (!response.ok) throw await parseError(response)
  return response.json() as Promise<MultiplayerGameView[]>
}

export function isParticipantView(
  v: MultiplayerGameView | MultiplayerGamePreview,
): v is MultiplayerGameView {
  return v.your_color !== null
}
