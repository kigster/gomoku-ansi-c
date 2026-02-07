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
         style={{ fontSize: '15pt', minHeight: '12rem' }}>
      {/* Player rows */}
      <div className="grid grid-cols-[auto_1fr] gap-x-4 gap-y-1">
        <span className="text-neutral-500">Player {blackLabel}:</span>
        <span className={`font-medium ${blackPlayer.includes(playerName) ? 'text-neutral-100' : 'text-neutral-400'}`}>
          {blackPlayer}
        </span>
        <span className="text-neutral-500">Player {whiteLabel}:</span>
        <span className={`font-medium ${whitePlayer.includes(playerName) ? 'text-neutral-100' : 'text-neutral-400'}`}>
          {whitePlayer}
        </span>
      </div>

      {/* Divider */}
      <div className="border-t border-neutral-700/60 my-2" />

      {/* Move info rows */}
      {isActive ? (
        <div className="grid grid-cols-[auto_1fr] gap-x-4 gap-y-1">
          <span className="text-neutral-500">Move Number:</span>
          <span className="text-neutral-200 font-mono">{moveCount + (phase === 'gameover' ? 0 : 1)}</span>
          <span className="text-neutral-500">Next Move:</span>
          <span className={`font-medium ${phase === 'thinking' ? 'text-amber-400 animate-pulse-slow' : 'text-neutral-300'}`}>
            {phase === 'gameover' ? '\u2014' : phase === 'thinking' ? 'AI is thinking\u2026' : nextPlayerName}
          </span>
        </div>
      ) : (
        <div className="grid grid-cols-[auto_1fr] gap-x-4 gap-y-1 invisible">
          <span>Move Number:</span><span>0</span>
          <span>Next Move:</span><span>—</span>
        </div>
      )}

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
