import { useCallback, useEffect, useRef, useState } from 'react'
import {
  getGame,
  postMove,
  resignGame,
  type MultiplayerGameView,
  type MultiplayerGamePreview,
} from '../lib/multiplayerClient'

// Initial poll interval, max poll interval after geometric backoff, and the
// hard wall-clock cap before we stop polling and surface "this game has
// expired" to the user. See doc/multiplayer-bugs.md item #5.
const BASE_POLL_INTERVAL_MS = 1500
const MAX_POLL_INTERVAL_MS = 8000
const MAX_AGE_WAITING_MS = 15 * 60 * 1000      // 15 min for `waiting` games
const MAX_AGE_IN_PROGRESS_MS = 8 * 60 * 60 * 1000 // 8 h for `in_progress`

export interface UseMultiplayerPollingResult {
  game: MultiplayerGameView | MultiplayerGamePreview | null
  isParticipant: boolean
  loading: boolean
  error: string | null
  /** True once the polling loop has timed out per `MAX_AGE_*_MS`. The UI
   *  should stop awaiting state changes and surface an "expired" message. */
  expired: boolean
  sendMove: (x: number, y: number) => Promise<void>
  sendResign: () => Promise<void>
  refresh: () => Promise<void>
}

/** Polls GET /multiplayer/{code} every 1.5s. When the server replies 304
 *  (no change since the prior `version`), keeps the existing state.
 *  Stops polling once the game reaches `finished` or `abandoned`. */
export function useMultiplayerPolling(
  token: string,
  code: string,
): UseMultiplayerPollingResult {
  const [game, setGame] = useState<
    MultiplayerGameView | MultiplayerGamePreview | null
  >(null)
  const [error, setError] = useState<string | null>(null)
  const [loading, setLoading] = useState(true)
  const [expired, setExpired] = useState(false)
  const versionRef = useRef<number | null>(null)
  const stoppedRef = useRef(false)
  // Mutable backoff interval — bumped on every 304/error, reset whenever the
  // server returns a fresh body.
  const intervalRef = useRef(BASE_POLL_INTERVAL_MS)
  // Track when polling started so we can enforce the wall-clock cap. The ref
  // is reset every time `code` changes.
  const startedAtRef = useRef<number>(Date.now())

  const refresh = useCallback(async () => {
    try {
      const result = await getGame(
        token,
        code,
        versionRef.current ?? undefined,
      )
      if (result === null) {
        // 304 — no change. Geometric backoff (capped at MAX_POLL_INTERVAL_MS).
        intervalRef.current = Math.min(
          intervalRef.current * 2,
          MAX_POLL_INTERVAL_MS,
        )
        return
      }
      setGame(result)
      versionRef.current = result.version
      setError(null)
      // Reset backoff on a fresh response.
      intervalRef.current = BASE_POLL_INTERVAL_MS
      if (
        result.state === 'finished' ||
        result.state === 'abandoned' ||
        result.state === 'cancelled'
      ) {
        stoppedRef.current = true
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err)
      setError(msg)
      // Back off on errors too — avoid hammering an unhealthy upstream.
      intervalRef.current = Math.min(
        intervalRef.current * 2,
        MAX_POLL_INTERVAL_MS,
      )
    } finally {
      setLoading(false)
    }
  }, [token, code])

  // Initial fetch + polling loop with backoff and a max-age cutoff.
  useEffect(() => {
    stoppedRef.current = false
    versionRef.current = null
    intervalRef.current = BASE_POLL_INTERVAL_MS
    startedAtRef.current = Date.now()
    setExpired(false)
    let cancelled = false

    const tick = async () => {
      if (cancelled || stoppedRef.current) return
      const cap =
        game?.state === 'in_progress'
          ? MAX_AGE_IN_PROGRESS_MS
          : MAX_AGE_WAITING_MS
      if (Date.now() - startedAtRef.current >= cap) {
        stoppedRef.current = true
        setExpired(true)
        return
      }
      await refresh()
      if (!cancelled && !stoppedRef.current) {
        setTimeout(tick, intervalRef.current)
      }
    }
    tick()

    return () => {
      cancelled = true
    }
    // game?.state is intentionally a read-only escape hatch for the cap; we
    // don't want to restart polling on every state change.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [refresh])

  const sendMove = useCallback(
    async (x: number, y: number) => {
      if (!game || game.your_color === null) return
      const expectedVersion = game.version
      try {
        const updated = await postMove(token, code, x, y, expectedVersion)
        setGame(updated)
        versionRef.current = updated.version
        setError(null)
      } catch (err) {
        const msg = err instanceof Error ? err.message : String(err)
        setError(msg)
        // On version conflict, force a fresh poll to catch up.
        await refresh()
      }
    },
    [game, token, code, refresh],
  )

  const sendResign = useCallback(async () => {
    try {
      const updated = await resignGame(token, code)
      setGame(updated)
      versionRef.current = updated.version
      setError(null)
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err)
      setError(msg)
    }
  }, [token, code])

  const isParticipant = game !== null && game.your_color !== null

  return { game, isParticipant, loading, error, expired, sendMove, sendResign, refresh }
}
