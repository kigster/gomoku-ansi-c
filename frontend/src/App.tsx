import { useState, useCallback, useEffect, useRef } from 'react'
import type { GameSettings } from './types'
import { DEFAULT_SETTINGS } from './constants'
import { useGameState } from './hooks/useGameState'
import { trackGameStart, trackGameFinish } from './analytics'
import NameModal from './components/NameModal'
import SettingsPanel from './components/SettingsPanel'
import Board from './components/Board'
import GameStatus from './components/GameStatus'
import ThinkingTimer from './components/ThinkingTimer'
import PreviousGames from './components/PreviousGames'
import logo from '../assets/images/logo.png'

const STORAGE_KEY = 'gomoku_player_name'
const STATS_KEY = 'gomoku_player_stats'
const HISTORY_KEY = 'gomoku_game_history'

interface PlayerStats {
  [name: string]: { won: number; lost: number }
}

export interface GameRecord {
  name: string
  result: 'won' | 'lost'
  humanTimeSec: number
  date: string
}

function loadStats(): PlayerStats {
  try {
    return JSON.parse(localStorage.getItem(STATS_KEY) || '{}')
  } catch {
    return {}
  }
}

function loadHistory(): GameRecord[] {
  try {
    return JSON.parse(localStorage.getItem(HISTORY_KEY) || '[]')
  } catch {
    return []
  }
}

function recordResult(name: string, won: boolean, humanTimeMs: number) {
  const stats = loadStats()
  if (!stats[name]) stats[name] = { won: 0, lost: 0 }
  if (won) stats[name].won++
  else stats[name].lost++
  localStorage.setItem(STATS_KEY, JSON.stringify(stats))

  const history = loadHistory()
  history.push({
    name,
    result: won ? 'won' : 'lost',
    humanTimeSec: Math.round(humanTimeMs / 1000),
    date: new Date().toLocaleDateString(),
  })
  localStorage.setItem(HISTORY_KEY, JSON.stringify(history))

  return { stats: stats[name], history }
}

export default function App() {
  const [playerName, setPlayerName] = useState<string | null>(
    () => localStorage.getItem(STORAGE_KEY)
  )

  const handleNameSubmit = useCallback((name: string) => {
    localStorage.setItem(STORAGE_KEY, name)
    setPlayerName(name)
    setStats(loadStats()[name] ?? null)
    setGameHistory(loadHistory())
  }, [])
  const [settings, setSettings] = useState<GameSettings>(DEFAULT_SETTINGS)
  const [showSettings, setShowSettings] = useState(false)
  const [stats, setStats] = useState<{ won: number; lost: number } | null>(
    () => {
      const name = localStorage.getItem(STORAGE_KEY)
      if (!name) return null
      return loadStats()[name] ?? null
    }
  )
  const [gameHistory, setGameHistory] = useState<GameRecord[]>(() => loadHistory())
  const prevPhaseRef = useRef<string>('idle')

  const {
    board,
    phase,
    error,
    lastMove,
    moveCount,
    winner,
    humanTimeMs,
    aiTimeMs,
    humanTotalMs,
    aiTotalMs,
    lastHumanMoveMs,
    lastAiMoveMs,
    turnStartMs,
    isHumanTurn,
    startGame,
    makeMove,
    undoMove,
    resetGame,
  } = useGameState(settings)

  // Record win/loss when game ends
  useEffect(() => {
    if (prevPhaseRef.current !== 'gameover' && phase === 'gameover' && playerName && winner !== 'draw') {
      const youWon = winner === settings.playerSide
      const { stats: updated, history } = recordResult(playerName, youWon, humanTimeMs)
      setStats(updated)
      setGameHistory(history)

      if (winner === 'X' || winner === 'O') {
        trackGameFinish(
          winner,
          settings.playerSide,
          playerName,
          Math.round(humanTimeMs / 1000),
          Math.round(aiTimeMs / 1000),
        )
      }
    }
    prevPhaseRef.current = phase
  }, [phase, winner, playerName, settings.playerSide, humanTimeMs, aiTimeMs])

  const [showHistory, setShowHistory] = useState(false)
  const historyBtnRef = useRef<HTMLButtonElement>(null)
  const isActive = phase === 'playing' || phase === 'thinking'

  if (!playerName) {
    return <NameModal onSubmit={handleNameSubmit} />
  }

  return (
    <div className="min-h-screen relative z-10">
      {/* Navigation Bar */}
      <nav className="bg-neutral-900/95 backdrop-blur-sm border-b border-neutral-800 shadow-lg">
        <div className="max-w-6xl mx-auto px-4 py-3 flex items-center justify-between">
          <div className="flex items-center gap-3">
            <img src={logo} alt="Gomoku" className="h-9 w-auto" />
            <h1 className="font-heading text-2xl font-bold text-amber-400">Gomoku</h1>
          </div>
          <button
            ref={historyBtnRef}
            onClick={() => setShowHistory(s => !s)}
            className="text-neutral-400 hover:text-neutral-200 transition-colors cursor-pointer"
          >
            Hello, <span className="text-neutral-200 font-medium">{playerName}</span>
            {gameHistory.length >= 2 && (
              <span className="ml-1 text-neutral-500 text-xs">{showHistory ? '\u25B2' : '\u25BC'}</span>
            )}
          </button>
        </div>
      </nav>

      {/* Previous Games dropdown — rendered outside nav to avoid backdrop-blur */}
      {showHistory && gameHistory.length >= 2 && (
        <>
          <div className="fixed inset-0 z-[998]" onClick={() => setShowHistory(false)} />
          <div className="fixed z-[999] w-80 shadow-2xl"
               style={{
                 top: (historyBtnRef.current?.getBoundingClientRect().bottom ?? 0) + 8,
                 right: window.innerWidth - (historyBtnRef.current?.getBoundingClientRect().right ?? 0),
               }}>
            <PreviousGames history={gameHistory} />
          </div>
        </>
      )}

      {/* Main Content */}
      <div className="flex justify-center px-2 sm:px-4 py-4 sm:py-8">
        <div className="game-panel rounded-2xl p-4 sm:p-8 max-w-5xl w-full text-neutral-100">
          <div className="flex flex-col lg:flex-row gap-4 sm:gap-8 items-center lg:items-start justify-center">
            {/* Left panel: Settings */}
            <div className="w-full lg:w-72 shrink-0 flex flex-col">
              <button
                onClick={() => setShowSettings(s => !s)}
                className="lg:hidden w-full mb-3 py-2 rounded-lg bg-neutral-700 hover:bg-neutral-600
                           font-medium transition-colors"
              >
                {showSettings ? 'Hide Settings' : 'Settings'}
              </button>
              <div className={`${showSettings ? 'block' : 'hidden'} lg:block`}>
                <SettingsPanel
                  settings={settings}
                  onChange={setSettings}
                  disabled={isActive}
                />
              </div>

              {/* Start / New Game Button */}
              <div className="mt-5">
                {phase === 'idle' && (
                  <>
                    <button
                      onClick={() => { setShowSettings(false); trackGameStart(settings); startGame(); }}
                      className="w-full py-4 rounded-xl text-xl font-bold font-heading
                                 bg-amber-600 hover:bg-amber-500 active:bg-amber-700
                                 shadow-lg shadow-amber-900/30 transition-all
                                 hover:shadow-xl hover:scale-[1.02]"
                    >
                      Start Game
                    </button>
                    <button
                      onClick={() => {
                        localStorage.removeItem(STORAGE_KEY)
                        setPlayerName(null)
                      }}
                      className="w-full mt-2 py-2 rounded-lg text-sm
                                 text-neutral-400 hover:text-neutral-200
                                 bg-neutral-800 hover:bg-neutral-700 transition-colors"
                    >
                      Change Human Player
                    </button>
                  </>
                )}
                {phase === 'gameover' && (
                  <button
                    onClick={resetGame}
                    className="w-full py-3 rounded-xl text-lg font-semibold font-heading
                               bg-neutral-700 hover:bg-neutral-600 transition-colors"
                  >
                    New Game
                  </button>
                )}
              </div>

              {/* Undo Button + Timer */}
              {isActive && (
                <div className="mt-auto pt-5">
                  {settings.undoEnabled && (
                    <button
                      onClick={undoMove}
                      disabled={phase !== 'playing' || moveCount < 2}
                      className="w-full py-3 rounded-xl text-lg font-bold font-heading
                                 bg-amber-600 hover:bg-amber-500 active:bg-amber-700
                                 shadow-lg shadow-amber-900/30 transition-all
                                 disabled:opacity-40 disabled:cursor-not-allowed"
                    >
                      Undo
                    </button>
                  )}
                  <ThinkingTimer phase={phase} playerName={playerName} />
                  <button
                    onClick={resetGame}
                    className="w-full mt-3 py-3 rounded-xl text-lg font-bold font-heading
                               bg-amber-600 hover:bg-amber-500 active:bg-amber-700
                               shadow-lg shadow-amber-900/30 transition-all"
                  >
                    Abort This Game
                  </button>
                </div>
              )}
            </div>

            {/* Center: Board + Status */}
            <div className="flex flex-col items-center w-full lg:w-auto">
              <GameStatus
                phase={phase}
                playerName={playerName}
                playerSide={settings.playerSide}
                displayMode={settings.displayMode}
                winner={winner}
                moveCount={moveCount}
                error={error}
                stats={stats}
                humanTotalMs={humanTotalMs}
                aiTotalMs={aiTotalMs}
                lastHumanMoveMs={lastHumanMoveMs}
                lastAiMoveMs={lastAiMoveMs}
                turnStartMs={turnStartMs}
                isHumanTurn={isHumanTurn}
              />
              <Board
                board={board}
                boardSize={settings.boardSize}
                displayMode={settings.displayMode}
                interactive={phase === 'playing'}
                lastMove={lastMove}
                onCellClick={makeMove}
              />
            </div>

          </div>
        </div>
      </div>

      {/* Footer */}
      <footer className="text-center py-6" style={{ fontSize: '12pt', fontWeight: 400 }}>
        <p className="text-neutral-500">
          &copy; 2026{' '}
          <a href="https://kig.re/" target="_blank" rel="noopener noreferrer"
             className="text-neutral-500 hover:text-amber-400 transition-colors">
            Konstantin Gredeskoul
          </a>
          , All Rights Reserved.
        </p>
        <p className="mt-1">
          <a href="https://github.com/kigster/gomoku-ansi-c"
             target="_blank" rel="noopener noreferrer"
             className="text-neutral-500 hover:text-amber-400 transition-colors inline-block"
          >
            <svg viewBox="0 0 16 16" width="30" height="30" fill="currentColor" aria-label="GitHub">
              <path d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 0 0016 8c0-4.42-3.58-8-8-8z"/>
            </svg>
          </a>
        </p>
      </footer>
    </div>
  )
}
