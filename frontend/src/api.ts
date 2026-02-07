import type { GameState } from './types'

const API_BASE = import.meta.env.VITE_API_BASE || ''

function delay(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms))
}

export async function postGameState(
  state: GameState,
  maxRetries = 20,
): Promise<GameState> {
  let delayMs = 100
  let attempt = 0

  while (true) {
    const response = await fetch(`${API_BASE}/gomoku/play`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(state),
    })

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
