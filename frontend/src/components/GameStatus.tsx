import { useState, useEffect } from 'react'
import type { GamePhase, PlayerSide, DisplayMode } from '../types'

interface GameStatusProps {
  phase: GamePhase
  playerName: string
  playerSide: PlayerSide
  displayMode: DisplayMode
  winner: string
  moveCount: number
  error: string | null
  stats: { won: number; lost: number } | null
  humanTotalMs: number
  aiTotalMs: number
  lastHumanMoveMs: number
  lastAiMoveMs: number
  turnStartMs: number
  isHumanTurn: boolean
}

function formatTime(ms: number): string {
  const totalSec = Math.max(0, Math.floor(ms / 1000))
  const min = Math.floor(totalSec / 60)
  const sec = totalSec % 60
  if (min === 0) return `${sec}s`
  return `${min}:${sec.toString().padStart(2, '0')}`
}

export default function GameStatus({
  phase,
  playerName,
  playerSide,
  displayMode,
  winner,
  moveCount,
  error,
  stats,
  humanTotalMs,
  aiTotalMs,
  lastHumanMoveMs,
  lastAiMoveMs,
  turnStartMs,
  isHumanTurn,
}: GameStatusProps) {
  const isStones = displayMode === 'stones'
  const blackLabel = isStones ? 'Black' : 'X'
  const whiteLabel = isStones ? 'White' : 'O'

  const blackPlayer = playerSide === 'X' ? `${playerName} (Human)` : 'AI'
  const whitePlayer = playerSide === 'O' ? `${playerName} (Human)` : 'AI'

  // X always moves first; even moveCount = X's turn, odd = O's turn
  const nextIsX = moveCount % 2 === 0
  const nextPlayerName = nextIsX
    ? (playerSide === 'X' ? `${playerName} (Human)` : 'AI')
    : (playerSide === 'O' ? `${playerName} (Human)` : 'AI')

  const isActive = phase === 'playing' || phase === 'thinking' || phase === 'gameover'

  // 1-second ticker for live timing display
  const [, setTick] = useState(0)
  useEffect(() => {
    if (phase !== 'playing' && phase !== 'thinking') return
    const interval = setInterval(() => setTick(t => t + 1), 1000)
    return () => clearInterval(interval)
  }, [phase])

  // Compute live timing values
  const now = Date.now()
  const currentMoveElapsed = turnStartMs > 0 ? now - turnStartMs : 0

  const humanCurrentMove = isHumanTurn ? currentMoveElapsed : lastHumanMoveMs
  const humanTotal = isHumanTurn ? humanTotalMs + currentMoveElapsed : humanTotalMs

  const isAiThinking = phase === 'thinking'
  const aiCurrentMove = isAiThinking ? currentMoveElapsed : lastAiMoveMs
  const aiTotal = isAiThinking ? aiTotalMs + currentMoveElapsed : aiTotalMs

  // Winner message
  let winnerText = ''
  if (phase === 'gameover') {
    const youWon = winner === playerSide
    const draw = winner === 'draw'
    if (draw) {
      winnerText = "It's a draw!"
    } else if (youWon) {
      winnerText = `${playerName} (Human) wins this game! You must be a Gomoku Master!`
    } else {
      const aiSideLabel = isStones
        ? (playerSide === 'X' ? 'White' : 'Black')
        : (playerSide === 'X' ? 'O' : 'X')
      winnerText = `AI (${aiSideLabel}) wins this game! Try again?`
    }
  }

  return (
    <div className="w-full mb-4 bg-neutral-800/80 rounded-xl px-6 py-4 backdrop-blur-sm"
         style={{ fontSize: '15pt' }}>
      {/* All info rows in a single grid for perfectly aligned colons */}
      <div className="grid grid-cols-[auto_auto_1fr] gap-x-2 gap-y-1">
        <span className="text-neutral-500 text-right whitespace-nowrap">Player {blackLabel}</span>
        <span className="text-neutral-500">:</span>
        <span className={`font-semibold ${blackPlayer.includes(playerName) ? 'text-neutral-100' : 'text-neutral-400'}`}>
          {blackPlayer}
        </span>

        <span className="text-neutral-500 text-right whitespace-nowrap">Player {whiteLabel}</span>
        <span className="text-neutral-500">:</span>
        <span className={`font-semibold ${whitePlayer.includes(playerName) ? 'text-neutral-100' : 'text-neutral-400'}`}>
          {whitePlayer}
        </span>

        {/* Divider */}
        <div className="col-span-3 border-t border-neutral-700/60 my-1" />

        {isActive ? (
          <>
            <span className="text-neutral-500 text-right whitespace-nowrap">Move Number</span>
            <span className="text-neutral-500">:</span>
            <span className="text-neutral-200 font-mono">{moveCount + (phase === 'gameover' ? 0 : 1)}</span>

            <span className="text-neutral-500 text-right whitespace-nowrap">Next Move</span>
            <span className="text-neutral-500">:</span>
            <span className={`font-semibold ${phase === 'thinking' ? 'text-amber-400 animate-pulse-slow' : 'text-neutral-300'}`}>
              {phase === 'gameover' ? '\u2014' : phase === 'thinking' ? 'AI is thinking\u2026' : nextPlayerName}
            </span>

            <span className="text-neutral-500 text-right whitespace-nowrap">Human Player Time</span>
            <span className="text-neutral-500">:</span>
            <span className="font-mono whitespace-nowrap">
              <span className={isHumanTurn ? 'text-amber-400' : 'text-neutral-400'}>{formatTime(humanCurrentMove)}</span>
              <span className="text-neutral-600 font-sans text-[13pt]"> (total this game: </span>
              <span className={isHumanTurn ? 'text-amber-400' : 'text-neutral-400'}>{formatTime(humanTotal)}</span>
              <span className="text-neutral-600 font-sans text-[13pt]">)</span>
            </span>

            <span className="text-neutral-500 text-right whitespace-nowrap">AI Player Time</span>
            <span className="text-neutral-500">:</span>
            <span className="font-mono whitespace-nowrap">
              <span className={isAiThinking ? 'text-amber-400' : 'text-neutral-400'}>{formatTime(aiCurrentMove)}</span>
              <span className="text-neutral-600 font-sans text-[13pt]"> (total this game: </span>
              <span className={isAiThinking ? 'text-amber-400' : 'text-neutral-400'}>{formatTime(aiTotal)}</span>
              <span className="text-neutral-600 font-sans text-[13pt]">)</span>
            </span>
          </>
        ) : (
          <>
            <span className="invisible whitespace-nowrap">Move Number</span>
            <span className="invisible">:</span>
            <span className="invisible">0</span>
            <span className="invisible whitespace-nowrap">Next Move</span>
            <span className="invisible">:</span>
            <span className="invisible">{'\u2014'}</span>
            <span className="invisible whitespace-nowrap">Human Player Time</span>
            <span className="invisible">:</span>
            <span className="invisible">0s (total this game: 0s)</span>
            <span className="invisible whitespace-nowrap">AI Player Time</span>
            <span className="invisible">:</span>
            <span className="invisible">0s (total this game: 0s)</span>
          </>
        )}
      </div>

      {/* Winner / error line — fixed slot */}
      <div className="mt-2 text-center" style={{ minHeight: '3em' }}>
        {phase === 'gameover' && (
          <>
            <span className="font-bold text-amber-400" style={{ fontSize: '15pt' }}>
              {winnerText}
            </span>
            {stats && stats.won + stats.lost > 0 && (
              <p className="text-neutral-400 mt-1" style={{ fontSize: '12pt' }}>
                You won {stats.won} out of {stats.won + stats.lost} games, that's{' '}
                {Math.round((stats.won / (stats.won + stats.lost)) * 100)}%
              </p>
            )}
          </>
        )}
        {error && (
          <span className="font-medium text-red-400">{error}</span>
        )}
        {phase === 'idle' && (
          <span className="text-neutral-500">Configure settings and press Start Game</span>
        )}
      </div>
    </div>
  )
}
