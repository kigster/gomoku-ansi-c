import type { GameRecord } from '../App'

interface PreviousGamesProps {
  history: GameRecord[]
  onClose: () => void
}

export default function PreviousGames({ history, onClose }: PreviousGamesProps) {
  const reversed = [...history].reverse()

  return (
    <div
      className="fixed inset-0 z-50 flex items-start sm:items-center justify-center pt-[5vh] sm:pt-0 bg-black/60 backdrop-blur-sm"
      onClick={onClose}
    >
      <div
        className="bg-neutral-800 rounded-2xl shadow-2xl max-w-5xl w-[95%] max-h-[80vh] relative
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
        <div className="overflow-y-auto px-8 py-4">
          {reversed.length === 0 ? (
            <p className="text-neutral-500 text-center py-8">No games played yet.</p>
          ) : (
            <table className="w-full">
              <thead>
                <tr className="text-neutral-500 text-sm border-b border-neutral-700">
                  <th className="text-left py-2 pr-3 font-medium">Player</th>
                  <th className="text-right py-2 pr-3 font-medium">Time</th>
                  <th className="text-left py-2 pr-3 font-medium">Result</th>
                  <th className="text-right py-2 pr-3 font-medium">Depth</th>
                  <th className="text-right py-2 font-medium">Radius</th>
                </tr>
              </thead>
              <tbody>
                {reversed.map((game, i) => (
                  <tr key={i} className="border-b border-neutral-700/40 text-sm">
                    <td className="py-2.5 pr-3 text-white">{game.name}</td>
                    <td className="py-2.5 pr-3 text-neutral-400 font-mono text-right">
                      {formatTime(game.humanTimeSec)}
                    </td>
                    <td className={`py-2.5 pr-3 font-semibold ${
                      game.result === 'won' ? 'text-amber-400' : 'text-red-400'
                    }`}>
                      {game.result === 'won' ? 'Won' : 'Lost'}
                    </td>
                    <td className="py-2.5 pr-3 text-neutral-400 font-mono text-right">
                      {game.depth ?? '—'}
                    </td>
                    <td className="py-2.5 text-neutral-400 font-mono text-right">
                      {game.radius ?? '—'}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>

        {/* Footer */}
        {reversed.length > 0 && (
          <div className="px-8 py-3 border-t border-neutral-700 text-neutral-500 text-xs text-center">
            Showing {reversed.length} game{reversed.length !== 1 ? 's' : ''} (max 100)
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

function formatTime(seconds: number): string {
  if (seconds < 60) return `${seconds}s`
  const m = Math.floor(seconds / 60)
  const s = seconds % 60
  return `${m}m ${s}s`
}
