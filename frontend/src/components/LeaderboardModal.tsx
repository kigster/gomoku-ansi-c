import { useState, useEffect } from 'react'

interface LeaderboardEntry {
  username: string
  score: number
  rating: number
  depth: number
  radius: number
  total_moves: number
  human_time_s: number
  geo_country: string | null
  geo_city: string | null
  played_at: string
}

interface LeaderboardModalProps {
  apiBase: string
  onClose: () => void
}

export default function LeaderboardModal ({
  apiBase,
  onClose
}: LeaderboardModalProps) {
  const [entries, setEntries] = useState<LeaderboardEntry[]>([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState('')

  useEffect(() => {
    fetch(`${apiBase}/leaderboard?limit=100`)
      .then(r => {
        if (!r.ok) throw new Error('Failed to load leaderboard')
        return r.json()
      })
      .then(data => setEntries(data.entries))
      .catch(e => setError(e.message))
      .finally(() => setLoading(false))
  }, [apiBase])

  return (
    <div className='fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm'>
      <div className='bg-neutral-800 rounded-2xl shadow-2xl p-6 max-w-3xl w-full mx-4 relative max-h-[85vh] flex flex-col'>
        <button
          type='button'
          onClick={onClose}
          className='absolute top-3 right-4 text-neutral-500 hover:text-neutral-200
                     text-2xl leading-none transition-colors'
          aria-label='Close'
        >
          &times;
        </button>

        <h2 className='text-2xl font-bold mb-1 text-center text-white'>
          Global{' '}
          <span className='font-heading text-amber-400'>Leaderboard</span>
        </h2>
        <p className='text-neutral-500 text-center mb-4 text-sm'>
          Top 100 players worldwide
        </p>

        {loading && (
          <p className='text-neutral-400 text-center py-8'>Loading...</p>
        )}

        {error && <p className='text-red-400 text-center py-8'>{error}</p>}

        {!loading && !error && entries.length === 0 && (
          <p className='text-neutral-400 text-center py-8'>
            No scores yet. Be the first to win a game!
          </p>
        )}

        {!loading && !error && entries.length > 0 && (
          <div className='overflow-y-auto flex-1'>
            <table className='w-full text-sm'>
              <thead className='sticky top-0 bg-neutral-800'>
                <tr className='text-neutral-400 border-b border-neutral-700'>
                  <th className='py-2 px-2 text-left w-10'>#</th>
                  <th className='py-2 px-2 text-left'>Player</th>
                  <th className='py-2 px-2 text-right'>Score</th>
                  <th className='py-2 px-2 text-right'>Rating</th>
                  <th className='py-2 px-2 text-center hidden sm:table-cell'>
                    Depth
                  </th>
                  <th className='py-2 px-2 text-right hidden sm:table-cell'>
                    Time
                  </th>
                  <th className='py-2 px-2 text-left hidden md:table-cell'>
                    Location
                  </th>
                </tr>
              </thead>
              <tbody>
                {entries.map((entry, i) => (
                  <tr
                    key={`${entry.username}-${i}`}
                    className={`border-b border-neutral-700/50 hover:bg-neutral-700/30 transition-colors
                      ${
                        i === 0
                          ? 'text-amber-400 font-semibold'
                          : i < 3
                          ? 'text-amber-200'
                          : 'text-neutral-300'
                      }`}
                  >
                    <td className='py-2 px-2'>
                      {i === 0
                        ? '\uD83E\uDD47'
                        : i === 1
                        ? '\uD83E\uDD48'
                        : i === 2
                        ? '\uD83E\uDD49'
                        : i + 1}
                    </td>
                    <td className='py-2 px-2 font-medium'>{entry.username}</td>
                    <td className='py-2 px-2 text-right tabular-nums'>
                      {entry.score.toLocaleString()}
                    </td>
                    <td className='py-2 px-2 text-right tabular-nums'>
                      {entry.rating}
                    </td>
                    <td className='py-2 px-2 text-center hidden sm:table-cell'>
                      d{entry.depth}/r{entry.radius}
                    </td>
                    <td className='py-2 px-2 text-right tabular-nums hidden sm:table-cell'>
                      {entry.human_time_s}s
                    </td>
                    <td className='py-2 px-2 text-neutral-500 hidden md:table-cell'>
                      {[entry.geo_city, entry.geo_country]
                        .filter(Boolean)
                        .join(', ') || '—'}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>
    </div>
  )
}
