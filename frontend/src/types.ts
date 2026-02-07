export interface PlayerConfig {
  player: 'human' | 'AI'
  depth?: number
  time_ms: number
}

export interface MoveEntry {
  'X (human)'?: [number, number]
  'X (AI)'?: [number, number]
  'O (human)'?: [number, number]
  'O (AI)'?: [number, number]
  time_ms?: number
  moves_searched?: number
  moves_evaluated?: number
  score?: number
  opponent?: number
  winner?: boolean
}

export interface GameState {
  X: PlayerConfig
  O: PlayerConfig
  board_size: 15 | 19
  radius: number
  timeout: string
  undo?: string
  winner: 'none' | 'X' | 'O' | 'draw'
  board_state: string[]
  moves: MoveEntry[]
}

export type DisplayMode = 'stones' | 'xo'
export type PlayerSide = 'X' | 'O'
export type GamePhase = 'idle' | 'playing' | 'thinking' | 'gameover'

export interface GameSettings {
  aiDepth: number
  aiRadius: number
  aiTimeout: string
  displayMode: DisplayMode
  playerSide: PlayerSide
  boardSize: 15 | 19
  undoEnabled: boolean
}

export type CellValue = 'empty' | 'X' | 'O'
