import { useState, useEffect } from 'react'

interface GameEntry {
  id: string
  player_name: string
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

export default function PreviousGames({ authToken, apiBase, onClose }: PreviousGamesProps) {
  const [games, setGames] = useState<GameEntry[]>([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState<string | null>(null)

  useEffect(() => {
    fetch(`${apiBase}/game/history?limit=100`, {
      headers: { 'Authorization': `Bearer ${authToken}` },
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
        headers: { 'Authorization': `Bearer ${authToken}` },
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
      // silently ignore download errors
    }
  }

  return (
    <div
      className="fixed inset-0 z-50 flex items-start sm:items-center justify-center pt-[5vh] sm:pt-0 bg-black/60 backdrop-blur-sm"
      onClick={onClose}
    >
      <div
        className="bg-neutral-800 rounded-2xl shadow-2xl w-[95%] max-w-6xl max-h-[80vh] relative
                   flex flex-col"
        onClick={e => e.stopPropagation()}
      >
        {/* Header */}
        <div className="flex items-center justify-between px-8 pt-6 pb-4 border-b border-neutral-700">
          <h2 className="text-2xl font-bold text-white font-heading">
            Game <span className="text-amber-400">History</span>
          </h2>
          <button
            onClick={onClose}
            className="text-neutral-500 hover:text-neutral-200 text-2xl leading-none
                       transition-colors"
            aria-label="Close"
          >
            &times;
          </button>
        </div>

        {/* Table */}
        <div className="overflow-y-auto overflow-x-auto px-8 py-4">
          {loading ? (
            <p className="text-neutral-500 text-center py-8">Loading...</p>
          ) : error ? (
            <p className="text-red-400 text-center py-8">Failed to load game history.</p>
          ) : games.length === 0 ? (
            <p className="text-neutral-500 text-center py-8">No games played yet.</p>
          ) : (
            <table className="w-full">
              <thead>
                <tr className="text-neutral-500 text-sm border-b border-neutral-700">
                  <th className="text-left py-2 pr-4 font-medium">Name</th>
                  <th className="text-left py-2 pr-4 font-medium">Status</th>
                  <th className="text-right py-2 pr-4 font-medium">Score</th>
                  <th className="text-right py-2 pr-4 font-medium">Time&nbsp;(s)</th>
                  <th className="text-right py-2 pr-4 font-medium">AI&nbsp;Time&nbsp;(s)</th>
                  <th className="text-right py-2 pr-4 font-medium">AI&nbsp;Depth</th>
                  <th className="text-right py-2 font-medium">Download</th>
                </tr>
              </thead>
              <tbody>
                {games.map(game => (
                  <tr key={game.id} className="border-b border-neutral-700/40 text-sm">
                    <td className="py-2.5 pr-4 text-white">{game.player_name}</td>
                    <td className={`py-2.5 pr-4 font-semibold ${
                      game.won ? 'text-amber-400' : 'text-red-400'
                    }`}>
                      {game.won ? 'Won' : 'Lost'}
                    </td>
                    <td className="py-2.5 pr-4 text-neutral-300 font-mono text-right">
                      {game.score}
                    </td>
                    <td className="py-2.5 pr-4 text-neutral-400 font-mono text-right">
                      {game.human_time_s.toFixed(1)}
                    </td>
                    <td className="py-2.5 pr-4 text-neutral-400 font-mono text-right">
                      {game.ai_time_s.toFixed(1)}
                    </td>
                    <td className="py-2.5 pr-4 text-neutral-400 font-mono text-right">
                      {game.depth}
                    </td>
                    <td className="py-2.5 text-right">
                      <button
                        onClick={() => handleDownload(game)}
                        className="text-amber-400 hover:text-amber-300 transition-colors
                                   text-xs underline cursor-pointer"
                      >
                        JSON
                      </button>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>

        {/* Footer */}
        {games.length > 0 && (
          <div className="px-8 py-3 border-t border-neutral-700 text-neutral-500 text-xs text-center">
            Showing {games.length} game{games.length !== 1 ? 's' : ''}
          </div>
        )}
        <div className="px-8 pb-6 pt-2">
          <button
            onClick={onClose}
            className="w-full py-3 rounded-xl text-lg font-bold font-heading
                       bg-green-600 hover:bg-green-500 active:bg-green-700
                       shadow-lg shadow-green-900/30 transition-all cursor-pointer"
          >
            Close
          </button>
        </div>
      </div>
    </div>
  )
}
