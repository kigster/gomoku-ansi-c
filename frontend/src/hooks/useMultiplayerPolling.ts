import { useCallback, useEffect, useRef, useState } from 'react'
import {
  getGame,
  postMove,
  resignGame,
  type MultiplayerGameView,
  type MultiplayerGamePreview,
} from '../lib/multiplayerClient'

const POLL_INTERVAL_MS = 1500

export interface UseMultiplayerPollingResult {
  game: MultiplayerGameView | MultiplayerGamePreview | null
  isParticipant: boolean
  loading: boolean
  error: string | null
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
  const versionRef = useRef<number | null>(null)
  const stoppedRef = useRef(false)

  const refresh = useCallback(async () => {
    try {
      const result = await getGame(
        token,
        code,
        versionRef.current ?? undefined,
      )
      if (result === null) return // 304 — no change
      setGame(result)
      versionRef.current = result.version
      setError(null)
      if (result.state === 'finished' || result.state === 'abandoned') {
        stoppedRef.current = true
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err)
      setError(msg)
    } finally {
      setLoading(false)
    }
  }, [token, code])

  // Initial fetch + polling loop.
  useEffect(() => {
    stoppedRef.current = false
    versionRef.current = null
    let cancelled = false

    const tick = async () => {
      if (cancelled || stoppedRef.current) return
      await refresh()
      if (!cancelled && !stoppedRef.current) {
        setTimeout(tick, POLL_INTERVAL_MS)
      }
    }
    tick()

    return () => {
      cancelled = true
    }
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

  return { game, isParticipant, loading, error, sendMove, sendResign, refresh }
}
