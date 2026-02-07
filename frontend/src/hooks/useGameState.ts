import { useState, useCallback, useRef } from 'react'
import type { GameState, GameSettings, GamePhase, CellValue } from '../types'
import { postGameState } from '../api'

function buildEmptyBoard(size: number): string[] {
  const row = Array(size).fill('.').join(' ')
  return Array(size).fill(row)
}

function updateBoardState(
  board: string[],
  row: number,
  col: number,
  piece: 'X' | 'O',
  boardSize: number,
): string[] {
  if (board.length === 0) {
    board = buildEmptyBoard(boardSize)
  }
  const cells = board[row].split(' ')
  cells[col] = piece
  const newBoard = [...board]
  newBoard[row] = cells.join(' ')
  return newBoard
}

function parseBoardState(boardState: string[], boardSize: number): CellValue[][] {
  if (boardState.length === 0) {
    return Array.from({ length: boardSize }, () =>
      Array(boardSize).fill('empty') as CellValue[]
    )
  }
  return boardState.map(row => {
    const cells = row.split(' ')
    return cells.map(c => {
      if (c === 'X') return 'X' as CellValue
      if (c === 'O') return 'O' as CellValue
      return 'empty' as CellValue
    })
  })
}

function getLastMoveCoords(state: GameState): [number, number] | null {
  if (state.moves.length === 0) return null
  const lastMove = state.moves[state.moves.length - 1]
  for (const key of ['X (human)', 'X (AI)', 'O (human)', 'O (AI)'] as const) {
    const coords = lastMove[key]
    if (coords) return coords
  }
  return null
}

function currentTurn(state: GameState): 'X' | 'O' {
  return state.moves.length % 2 === 0 ? 'X' : 'O'
}

export function useGameState(settings: GameSettings) {
  const [gameState, setGameState] = useState<GameState | null>(null)
  const [phase, setPhase] = useState<GamePhase>('idle')
  const [error, setError] = useState<string | null>(null)
  const moveStartTime = useRef<number>(0)
  const humanTimeAccum = useRef<number>(0)

  const buildInitialState = useCallback((): GameState => {
    const humanSide = settings.playerSide
    return {
      X: {
        player: humanSide === 'X' ? 'human' : 'AI',
        depth: humanSide === 'X' ? 0 : settings.aiDepth,
        time_ms: 0,
      },
      O: {
        player: humanSide === 'O' ? 'human' : 'AI',
        depth: humanSide === 'O' ? 0 : settings.aiDepth,
        time_ms: 0,
      },
      board_size: settings.boardSize,
      radius: settings.aiRadius,
      timeout: settings.aiTimeout,
      undo: settings.undoEnabled ? 'on' : undefined,
      winner: 'none',
      board_state: [],
      moves: [],
    }
  }, [settings])

  const sendToServer = useCallback(async (state: GameState) => {
    setPhase('thinking')
    setError(null)
    try {
      const response = await postGameState(state)
      setGameState(response)
      if (response.winner !== 'none') {
        setPhase('gameover')
      } else {
        moveStartTime.current = Date.now()
        setPhase('playing')
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error')
      setPhase('playing')
    }
  }, [])

  const startGame = useCallback(async () => {
    const initial = buildInitialState()
    setGameState(initial)
    setError(null)
    humanTimeAccum.current = 0

    if (settings.playerSide === 'O') {
      // AI plays first (X), send empty state to server
      await sendToServer(initial)
    } else {
      // Human plays first, wait for click
      moveStartTime.current = Date.now()
      setPhase('playing')
    }
  }, [buildInitialState, settings.playerSide, sendToServer])

  const makeMove = useCallback(async (row: number, col: number) => {
    if (!gameState || phase !== 'playing') return
    if (gameState.winner !== 'none') return

    const board = parseBoardState(gameState.board_state, gameState.board_size)
    if (board[row][col] !== 'empty') return

    const turn = currentTurn(gameState)
    if (turn !== settings.playerSide) return

    const elapsed = Date.now() - moveStartTime.current
    humanTimeAccum.current += elapsed
    const moveKey = `${turn} (human)` as 'X (human)' | 'O (human)'

    const newBoardState = updateBoardState(
      gameState.board_state,
      row,
      col,
      turn,
      gameState.board_size,
    )

    const newState: GameState = {
      ...gameState,
      board_state: newBoardState,
      moves: [
        ...gameState.moves,
        { [moveKey]: [row, col] as [number, number], time_ms: elapsed },
      ],
    }

    // Optimistically update UI with human's move
    setGameState(newState)

    // Send to server for AI's response
    await sendToServer(newState)
  }, [gameState, phase, settings.playerSide, sendToServer])

  const board = gameState
    ? parseBoardState(gameState.board_state, gameState.board_size)
    : parseBoardState([], settings.boardSize)

  const lastMove = gameState ? getLastMoveCoords(gameState) : null
  const moveCount = gameState ? gameState.moves.length : 0
  const winner = gameState?.winner ?? 'none'

  const undoMove = useCallback(() => {
    if (!gameState || phase !== 'playing') return
    // Remove last 2 moves (AI + human) to get back to human's turn
    const movesToRemove = gameState.moves.length >= 2 ? 2 : gameState.moves.length
    if (movesToRemove === 0) return
    const newMoves = gameState.moves.slice(0, -movesToRemove)
    // Rebuild board from remaining moves
    let newBoard: string[] = []
    for (const move of newMoves) {
      for (const key of ['X (human)', 'X (AI)', 'O (human)', 'O (AI)'] as const) {
        const coords = move[key]
        if (coords) {
          const piece = key.startsWith('X') ? 'X' as const : 'O' as const
          newBoard = updateBoardState(newBoard, coords[0], coords[1], piece, gameState.board_size)
        }
      }
    }
    setGameState({
      ...gameState,
      board_state: newBoard,
      moves: newMoves,
      winner: 'none',
    })
    moveStartTime.current = Date.now()
  }, [gameState, phase])

  const humanTimeMs = humanTimeAccum.current

  const resetGame = useCallback(() => {
    setGameState(null)
    setPhase('idle')
    setError(null)
  }, [])

  return {
    board,
    phase,
    error,
    lastMove,
    moveCount,
    winner,
    humanTimeMs,
    startGame,
    makeMove,
    undoMove,
    resetGame,
  }
}
