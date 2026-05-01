import { useState, useEffect } from 'react'
import ModalShell from './ModalShell'

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
    <ModalShell
      title={<>Worldwide <span className='text-amber-400'>Leaderboard</span></>}
      onClose={onClose}
      widthClassName='max-w-4xl'
      bodyClassName='space-y-4'
    >
      <p className='text-center text-sm text-neutral-500'>
        Top 100 players worldwide
      </p>
      <p className='text-center text-xs italic text-neutral-500'>
        NOTE: Only games of humans vs AI are counted.
      </p>

      {loading && (
        <p className='py-8 text-center text-neutral-400'>Loading...</p>
      )}

      {error && <p className='py-8 text-center text-red-400'>{error}</p>}

      {!loading && !error && entries.length === 0 && (
        <p className='py-8 text-center text-neutral-400'>
          No scores yet. Be the first to win a game.
        </p>
      )}

      {!loading && !error && entries.length > 0 && (
        <div className='overflow-x-auto'>
          <table className='w-full text-sm'>
            <thead className='sticky top-0 bg-neutral-800'>
              <tr className='border-b border-neutral-700 text-neutral-400'>
                <th className='w-10 px-2 py-2 text-left'>#</th>
                <th className='px-2 py-2 text-left'>Player</th>
                <th className='px-2 py-2 text-right'>Score</th>
                <th className='px-2 py-2 text-right'>Rating</th>
                <th className='hidden px-2 py-2 text-center sm:table-cell'>Depth</th>
                <th className='hidden px-2 py-2 text-right sm:table-cell'>Time</th>
                <th className='hidden px-2 py-2 text-left md:table-cell'>Location</th>
              </tr>
            </thead>
            <tbody>
              {entries.map((entry, i) => (
                <tr
                  key={`${entry.username}-${i}`}
                  className={`border-b border-neutral-700/50 transition-colors hover:bg-neutral-700/30 ${
                    i === 0
                      ? 'font-semibold text-amber-400'
                      : i < 3
                        ? 'text-amber-200'
                        : 'text-neutral-300'
                  }`}
                >
                  <td className='px-2 py-2'>{i + 1}</td>
                  <td className='px-2 py-2 font-medium'>{entry.username}</td>
                  <td className='px-2 py-2 text-right tabular-nums'>{entry.score.toLocaleString()}</td>
                  <td className='px-2 py-2 text-right tabular-nums'>{entry.rating}</td>
                  <td className='hidden px-2 py-2 text-center sm:table-cell'>d{entry.depth}/r{entry.radius}</td>
                  <td className='hidden px-2 py-2 text-right tabular-nums sm:table-cell'>{entry.human_time_s}s</td>
                  <td className='hidden px-2 py-2 text-neutral-500 md:table-cell'>
                    {[entry.geo_city, entry.geo_country].filter(Boolean).join(', ') || '—'}
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
