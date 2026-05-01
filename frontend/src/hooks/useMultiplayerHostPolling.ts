import { useEffect, useRef, useState } from 'react'
import {
  getGame,
  type MultiplayerGameView,
  type MultiplayerGamePreview,
} from '../lib/multiplayerClient'

interface Args {
  /** Auth token. Polling pauses when null. */
  token: string | null
  /** Game code to poll. Polling pauses when null. */
  code: string | null
  /** Stop polling when state ≠ 'waiting'. Default: true. */
  stopWhenNotWaiting?: boolean
  /** Hard cap (ms) — stop after this elapsed wall time even if state is still 'waiting'. Default: 15 minutes. */
  maxAgeMs?: number
  /** Initial poll interval (ms). Default: 100. */
  baseIntervalMs?: number
  /** Maximum backoff interval (ms). Default: 60000 (1 min) — safety cap so the geometric series never single-sleeps the entire 15-min budget. */
  maxIntervalMs?: number
  /** Backoff multiplier applied after each 304 / error. Default: 1.5. */
  backoffFactor?: number
}

interface State {
  /** Most recent server view (full or preview). */
  view: MultiplayerGameView | MultiplayerGamePreview | null
  /** Seconds since polling started. */
  secondsWaited: number
  /** True once we've stopped polling because we hit `maxAgeMs`. */
  expired: boolean
  /** Polling-network error (transient). */
  error: string | null
}

/**
 * Polls `/multiplayer/{code}` for the **host** while they wait for a guest.
 * Implements the polling backoff requested by `doc/multiplayer-bugs.md` #5:
 * after each consecutive 304 the interval doubles up to `maxIntervalMs`,
 * resetting whenever the server sends a fresh body.
 */
export function useMultiplayerHostPolling ({
  token,
  code,
  stopWhenNotWaiting = true,
  maxAgeMs = 15 * 60 * 1000,
  baseIntervalMs = 100,
  maxIntervalMs = 60_000,
  backoffFactor = 1.5,
}: Args): State {
  const [view, setView] = useState<MultiplayerGameView | MultiplayerGamePreview | null>(null)
  const [secondsWaited, setSecondsWaited] = useState(0)
  const [expired, setExpired] = useState(false)
  const [error, setError] = useState<string | null>(null)

  // Tick the seconds counter once a second.
  useEffect(() => {
    if (!token || !code) return
    setSecondsWaited(0)
    const t = setInterval(() => setSecondsWaited(s => s + 1), 1000)
    return () => clearInterval(t)
  }, [token, code])

  // Poll loop with backoff.
  const cancelledRef = useRef(false)
  useEffect(() => {
    cancelledRef.current = false
    if (!token || !code) return

    let lastVersion: number | undefined = undefined
    let intervalMs = baseIntervalMs
    let timeout: ReturnType<typeof setTimeout> | null = null
    const startedAt = Date.now()

    const tick = async () => {
      if (cancelledRef.current) return
      if (Date.now() - startedAt >= maxAgeMs) {
        setExpired(true)
        return
      }
      try {
        const v = await getGame(token, code, lastVersion)
        if (cancelledRef.current) return
        if (v === null) {
          // 304 — no change. Back off geometrically (capped).
          intervalMs = Math.min(intervalMs * backoffFactor, maxIntervalMs)
        } else {
          lastVersion = v.version
          setView(v)
          setError(null)
          intervalMs = baseIntervalMs
          if (stopWhenNotWaiting && v.state !== 'waiting') return
        }
      } catch (err) {
        if (!cancelledRef.current) {
          setError(err instanceof Error ? err.message : 'poll_failed')
          intervalMs = Math.min(intervalMs * backoffFactor, maxIntervalMs)
        }
      }
      if (!cancelledRef.current) {
        timeout = setTimeout(tick, intervalMs)
      }
    }

    timeout = setTimeout(tick, intervalMs)
    return () => {
      cancelledRef.current = true
      if (timeout) clearTimeout(timeout)
    }
  }, [token, code, stopWhenNotWaiting, maxAgeMs, baseIntervalMs, maxIntervalMs])

  return { view, secondsWaited, expired, error }
}
