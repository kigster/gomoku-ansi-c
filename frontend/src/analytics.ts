import type { GameSettings } from './types'

declare global {
  interface Window {
    gtag?: (...args: unknown[]) => void
  }
}

function gtag(command: string, action: string, params?: Record<string, unknown>) {
  if (window.gtag) {
    window.gtag(command, action, params)
  }
}

let gameNumber = 0

export function trackEntered(name: string) {
  gtag('event', 'entered', { name })
}

export function trackAbandoned() {
  gtag('event', 'abandoned', {})
}

export function trackGameStart(settings: GameSettings) {
  gameNumber++
  gtag('event', 'game.start', {
    depth: settings.aiDepth,
    radius: settings.aiRadius,
    timeout: settings.aiTimeout,
    undo: settings.undoEnabled,
    display: settings.displayMode,
    human_plays: settings.playerSide === 'X' ? 'black' : 'white',
    game_number: gameNumber,
  })
}

export function trackTimeout(configuredTimeoutSec: number) {
  gtag('event', 'timeout', {
    configured_timeout_sec: configuredTimeoutSec,
  })
}

export function trackCriticalTimeout(attempts: number) {
  gtag('event', 'critical_timeout', {
    number_of_attempts: attempts,
  })
}

export function trackGameFinish(
  winnerSide: 'X' | 'O',
  playerSide: 'X' | 'O',
  playerName: string,
  humanTimeSec: number,
  aiTimeSec: number,
) {
  const winnerIsHuman = winnerSide === playerSide
  const winnerColor = winnerSide === 'X' ? 'black' : 'white'
  const loserColor = winnerSide === 'X' ? 'white' : 'black'

  gtag('event', 'game.finish', {
    winner: {
      color: winnerColor,
      time_seconds: winnerIsHuman ? humanTimeSec : aiTimeSec,
      player: winnerIsHuman ? `${playerName} (Human)` : 'AI',
    },
    loser: {
      color: loserColor,
      time_seconds: winnerIsHuman ? aiTimeSec : humanTimeSec,
      player: winnerIsHuman ? 'AI' : `${playerName} (Human)`,
    },
  })
}
