import { useCallback, useEffect, useMemo, useState } from 'react'
import Board from './Board'
import WaitingForOpponent from './WaitingForOpponent'
import {
  joinGame,
  isParticipantView,
  type MultiplayerGameView,
} from '../lib/multiplayerClient'
import { useMultiplayerPolling } from '../hooks/useMultiplayerPolling'
import type { CellValue } from '../types'

interface MultiplayerGamePageProps {
  token: string
  code: string
  username: string
}

function buildBoard(
  size: number,
  moves: [number, number][],
): CellValue[][] {
  const board: CellValue[][] = Array.from({ length: size }, () =>
    Array<CellValue>(size).fill('empty'),
  )
  moves.forEach(([x, y], idx) => {
    if (x < 0 || x >= size || y < 0 || y >= size) return
    board[x][y] = idx % 2 === 0 ? 'X' : 'O'
  })
  return board
}

export default function MultiplayerGamePage({
  token,
  code,
  username,
}: MultiplayerGamePageProps) {
  const { game, loading, error, sendMove, sendResign, refresh } =
    useMultiplayerPolling(token, code)
  const [joining, setJoining] = useState(false)
  const [joinError, setJoinError] = useState<string | null>(null)

  // If the loaded game is in `waiting` state and the caller isn't the host,
  // automatically POST /join.
  useEffect(() => {
    if (!game || joining) return
    if (game.state !== 'waiting') return
    if (game.your_color !== null) return
    if (game.host.username === username) return
    let cancelled = false
    setJoining(true)
    joinGame(token, code)
      .then(() => {
        if (cancelled) return
        return refresh()
      })
      .catch((err) => {
        if (cancelled) return
        setJoinError(err instanceof Error ? err.message : String(err))
      })
      .finally(() => {
        if (!cancelled) setJoining(false)
      })
    return () => {
      cancelled = true
    }
  }, [game, joining, token, code, username, refresh])

  const handleCellClick = useCallback(
    (row: number, col: number) => {
      if (!game || !isParticipantView(game)) return
      if (!game.your_turn) return
      void sendMove(row, col)
    },
    [game, sendMove],
  )

  const board = useMemo(() => {
    if (!game) return null
    const moves = isParticipantView(game) ? game.moves : []
    return buildBoard(game.board_size, moves)
  }, [game])

  if (loading && !game) {
    return (
      <div className="min-h-screen flex items-center justify-center text-neutral-300">
        Loading game…
      </div>
    )
  }

  if (!game) {
    return (
      <div className="min-h-screen flex flex-col items-center justify-center text-neutral-300 gap-4">
        <p>Could not load game.</p>
        {error && <p className="text-red-400 text-sm">{error}</p>}
        <a
          href="/"
          className="px-4 py-2 rounded-lg bg-amber-600 hover:bg-amber-500 text-neutral-900 font-semibold"
        >
          Back home
        </a>
      </div>
    )
  }

  const interactive =
    isParticipantView(game) &&
    game.state === 'in_progress' &&
    game.your_turn

  const lastMove: [number, number] | null = (() => {
    if (!isParticipantView(game) || game.moves.length === 0) return null
    const m = game.moves[game.moves.length - 1]
    return [m[0], m[1]]
  })()

  return (
    <div className="min-h-screen px-4 py-6 text-neutral-100">
      <div className="max-w-3xl mx-auto flex flex-col items-center gap-4">
        <h1 className="font-heading text-3xl font-bold text-amber-400">
          Gomoku — Game {game.code}
        </h1>

        {game.state === 'waiting' && (
          <WaitingForOpponent code={game.code} />
        )}

        {(game.state === 'in_progress' || game.state === 'finished') && (
          <>
            <PlayerHeader game={game as MultiplayerGameView} />
            {board && (
              <Board
                board={board}
                boardSize={game.board_size === 19 ? 19 : 15}
                displayMode="stones"
                interactive={interactive}
                lastMove={lastMove}
                onCellClick={handleCellClick}
              />
            )}
            {isParticipantView(game) && game.state === 'in_progress' && (
              <div className="flex flex-col items-center gap-2 mt-2">
                <p className="text-neutral-300">
                  {game.your_turn
                    ? 'Your move.'
                    : 'Waiting for opponent…'}
                </p>
                <button
                  onClick={() => {
                    if (window.confirm('Resign this game?')) void sendResign()
                  }}
                  className="px-4 py-2 rounded-lg bg-red-700 hover:bg-red-600 text-white font-semibold"
                >
                  Resign
                </button>
              </div>
            )}
          </>
        )}

        {game.state === 'finished' && (
          <div className="mt-4 px-6 py-4 rounded-xl bg-neutral-800 border border-amber-500 text-center">
            <h2 className="font-heading text-2xl text-amber-400 font-bold mb-1">
              Game over
            </h2>
            <p className="text-neutral-200">
              Winner: <strong>{game.winner ?? 'draw'}</strong>
            </p>
            <a
              href="/"
              className="inline-block mt-3 px-4 py-2 rounded-lg bg-amber-600 hover:bg-amber-500 text-neutral-900 font-semibold"
            >
              Back home
            </a>
          </div>
        )}

        {error && (
          <p className="text-red-400 text-sm mt-2 max-w-md text-center">
            {error}
          </p>
        )}
        {joinError && (
          <p className="text-red-400 text-sm mt-2">{joinError}</p>
        )}
      </div>
    </div>
  )
}

function PlayerHeader({ game }: { game: MultiplayerGameView }) {
  const yourSide = game.your_color
  const hostLabel = `${game.host.username} (${game.host.color})`
  const guestLabel = game.guest
    ? `${game.guest.username} (${game.guest.color})`
    : '— waiting —'
  return (
    <div className="flex flex-wrap gap-4 justify-center text-neutral-300 text-sm">
      <div>
        Host: <span className="text-amber-300 font-semibold">{hostLabel}</span>
      </div>
      <div>
        Guest: <span className="text-amber-300 font-semibold">{guestLabel}</span>
      </div>
      {yourSide && (
        <div>
          You play: <span className="text-amber-300 font-semibold">{yourSide}</span>
        </div>
      )}
    </div>
  )
}
