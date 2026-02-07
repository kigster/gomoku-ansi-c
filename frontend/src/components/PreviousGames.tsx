import type { GameRecord } from '../App'

interface PreviousGamesProps {
  history: GameRecord[]
}

export default function PreviousGames({ history }: PreviousGamesProps) {
  // Show most recent games first
  const reversed = [...history].reverse()

  return (
    <div className="bg-neutral-800 rounded-xl p-6"
         style={{ fontSize: '14pt', fontWeight: 600 }}>
      <h3 className="font-heading text-xl font-bold text-amber-400 mb-4">Previous Games</h3>
      <div className="overflow-y-auto" style={{ maxHeight: '500px' }}>
        <table className="w-full">
          <thead>
            <tr className="text-neutral-500 border-b border-neutral-700">
              <th className="text-left py-2 pr-3">Player</th>
              <th className="text-left py-2 pr-3">Time</th>
              <th className="text-left py-2">Result</th>
            </tr>
          </thead>
          <tbody>
            {reversed.map((game, i) => (
              <tr key={i} className="border-b border-neutral-700/40">
                <td className="py-2 pr-3 text-white">{game.name}</td>
                <td className="py-2 pr-3 text-neutral-400 font-mono">{game.humanTimeSec}s</td>
                <td className={`py-2 ${game.result === 'won' ? 'text-amber-400' : 'text-red-400'}`}>
                  {game.result === 'won' ? 'Won' : 'Lost'}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  )
}
