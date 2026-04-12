import { useState, useEffect } from 'react'
import ModalShell from './ModalShell'

interface GameEntry {
  id: string
  username: string
  won: boolean
  score: number
  depth: number
  human_time_s: number
  ai_time_s: number
  played_at: string
}

interface PreviousGamesProps {
  authToken: string
  apiBase: string
  onClose: () => void
}

export default function PreviousGames ({
  authToken,
  apiBase,
  onClose
}: PreviousGamesProps) {
  const [games, setGames] = useState<GameEntry[]>([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState<string | null>(null)

  useEffect(() => {
    fetch(`${apiBase}/game/history?limit=100`, {
      headers: { Authorization: `Bearer ${authToken}` }
    })
      .then(r => {
        if (!r.ok) throw new Error(`${r.status}`)
        return r.json()
      })
      .then(data => setGames(data.games))
      .catch(e => setError(e.message))
      .finally(() => setLoading(false))
  }, [authToken, apiBase])

  const handleDownload = async (game: GameEntry) => {
    try {
      const resp = await fetch(`${apiBase}/game/${game.id}/json`, {
        headers: { Authorization: `Bearer ${authToken}` }
      })
      if (!resp.ok) throw new Error('Download failed')
      const data = await resp.json()
      const pretty = JSON.stringify(data, null, 2)
      const blob = new Blob([pretty], { type: 'application/json' })
      const url = URL.createObjectURL(blob)
      const a = document.createElement('a')
      a.href = url
      a.download = `gomoku-${game.played_at.slice(0, 10)}-depth${game.depth}.json`
      a.click()
      URL.revokeObjectURL(url)
    } catch {
      // Silently ignore download errors.
    }
  }

  return (
    <ModalShell
      title={<>Your Game <span className='text-amber-400'>History</span></>}
      onClose={onClose}
      widthClassName='max-w-7xl'
      bodyClassName='space-y-4'
      footer={
        <div className='space-y-3'>
          {games.length > 0 && (
            <div className='text-center text-xs text-neutral-500'>
              Showing {games.length} game{games.length !== 1 ? 's' : ''}
            </div>
          )}
          <button
            onClick={onClose}
            className='w-full rounded-xl bg-green-600 py-3 text-lg font-bold font-heading
                       shadow-lg shadow-green-900/30 transition-all cursor-pointer
                       hover:bg-green-500 active:bg-green-700'
          >
            Close
          </button>
        </div>
      }
    >
      {loading ? (
        <p className='py-8 text-center text-neutral-500'>Loading...</p>
      ) : error ? (
        <p className='py-8 text-center text-red-400'>Failed to load game history.</p>
      ) : games.length === 0 ? (
        <p className='py-8 text-center text-neutral-500'>No games played yet.</p>
      ) : (
        <div className='overflow-x-auto'>
          <table className='w-full'>
            <thead>
              <tr className='border-b border-neutral-700 text-[14px] text-neutral-500'>
                <th className='py-2 pr-4 text-left font-medium'>Username</th>
                <th className='py-2 pr-4 text-left font-medium'>Date</th>
                <th className='py-2 pr-4 text-left font-medium'>Status</th>
                <th className='py-2 pr-4 text-right font-medium'>Your Score</th>
                <th className='py-2 pr-4 text-right font-medium'>Your Time (s)</th>
                <th className='py-2 pr-4 text-right font-medium'>AI Time (s)</th>
                <th className='py-2 pr-4 text-right font-medium'>AI Depth</th>
                <th className='py-2 text-right font-medium'>Download Game</th>
              </tr>
            </thead>
            <tbody>
              {games.map(game => (
                <tr
                  key={game.id}
                  className='border-b border-neutral-700/40 text-[14px]'
                >
                  <td className='py-2.5 pr-4 text-white'>{game.username}</td>
                  <td className='whitespace-nowrap py-2.5 pr-4 text-neutral-400'>
                    {new Date(game.played_at).toLocaleDateString('en-US', {
                      month: '2-digit',
                      day: '2-digit',
                      year: 'numeric'
                    })}
                  </td>
                  <td className={`py-2.5 pr-4 font-semibold ${game.won ? 'text-amber-400' : 'text-red-400'}`}>
                    {game.won ? 'Won' : 'Lost'}
                  </td>
                  <td className='py-2.5 pr-4 text-right font-mono text-neutral-300'>{game.score}</td>
                  <td className='py-2.5 pr-4 text-right font-mono text-neutral-400'>{game.human_time_s.toFixed(1)}</td>
                  <td className='py-2.5 pr-4 text-right font-mono text-neutral-400'>{game.ai_time_s.toFixed(1)}</td>
                  <td className='py-2.5 pr-4 text-right font-mono text-neutral-400'>{game.depth}</td>
                  <td className='py-2.5 text-right'>
                    <button
                      onClick={() => handleDownload(game)}
                      className='cursor-pointer text-xs text-amber-400 underline transition-colors hover:text-amber-300'
                    >
                      JSON
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </ModalShell>
  )
}
