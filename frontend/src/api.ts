import type { GameState } from './types'

const API_BASE = import.meta.env.VITE_API_BASE || ''

function delay(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms))
}

export async function postGameState(
  state: GameState,
  maxRetries = 20,
  timeoutMs?: number,
): Promise<GameState> {
  let delayMs = 100
  let attempt = 0

  const url = `${API_BASE}/gomoku/play`

  while (true) {
    let response: Response
    try {
      const options: RequestInit = {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(state),
      }
      if (timeoutMs) {
        options.signal = AbortSignal.timeout(timeoutMs)
      }
      response = await fetch(url, options)
    } catch (err) {
      if (err instanceof DOMException && err.name === 'TimeoutError') {
        throw err
      }
      const msg = err instanceof Error ? err.message : String(err)
      const cause = err instanceof Error && err.cause ? ` cause: ${JSON.stringify(err.cause)}` : ''
      throw new Error(`Network error posting to ${url}: ${msg}${cause} (type: ${err instanceof Error ? err.constructor.name : typeof err})`)
    }

    if (response.ok) {
      return response.json()
    }

    if (response.status === 503) {
      attempt++
      if (attempt >= maxRetries) {
        throw new Error('Server busy: max retries exceeded')
      }
      await delay(delayMs)
      delayMs = Math.min(delayMs * 2, 10000)
      continue
    }

    const text = await response.text().catch(() => '')
    throw new Error(`Server error ${response.status}: ${text}`)
  }
}

export async function checkHealth(): Promise<boolean> {
  try {
    const response = await fetch(`${API_BASE}/health`)
    return response.ok
  } catch {
    return false
  }
}
