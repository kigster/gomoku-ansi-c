import type { GameSettings, DisplayMode, PlayerSide } from '../types'
import { TIMEOUT_OPTIONS } from '../constants'

interface SettingsPanelProps {
  settings: GameSettings
  onChange: (settings: GameSettings) => void
  disabled: boolean
}

export default function SettingsPanel({ settings, onChange, disabled }: SettingsPanelProps) {
  const update = (patch: Partial<GameSettings>) => {
    onChange({ ...settings, ...patch })
  }

  const isStones = settings.displayMode === 'stones'

  return (
    <div className="bg-neutral-800/90 rounded-xl p-6 backdrop-blur-sm">
      <h3 className="font-heading text-xl font-bold text-amber-400 mb-5">Settings</h3>

      <table className="w-full" style={{ borderSpacing: '0 8px', borderCollapse: 'separate' }}>
        <tbody>
          {/* AI Search Depth */}
          <tr>
            <td className="text-neutral-300 pr-4 py-1 whitespace-nowrap align-middle">AI Depth</td>
            <td className="py-1">
              <div className="flex items-center gap-3">
                <input type="range" min={2} max={5}
                  value={settings.aiDepth}
                  onChange={e => update({ aiDepth: Number(e.target.value) })}
                  disabled={disabled}
                  className="flex-1 accent-amber-500"
                />
                <span className="font-mono text-amber-300 w-5 text-center">{settings.aiDepth}</span>
              </div>
            </td>
          </tr>

          {/* AI Search Radius */}
          <tr>
            <td className="text-neutral-300 pr-4 py-1 whitespace-nowrap align-middle">AI Radius</td>
            <td className="py-1">
              <div className="flex items-center gap-3">
                <input type="range" min={1} max={4}
                  value={settings.aiRadius}
                  onChange={e => update({ aiRadius: Number(e.target.value) })}
                  disabled={disabled}
                  className="flex-1 accent-amber-500"
                />
                <span className="font-mono text-amber-300 w-5 text-center">{settings.aiRadius}</span>
              </div>
            </td>
          </tr>

          {/* AI Timeout */}
          <tr>
            <td className="text-neutral-300 pr-4 py-1 whitespace-nowrap align-middle">AI Timeout</td>
            <td className="py-1">
              <select
                value={settings.aiTimeout}
                onChange={e => update({ aiTimeout: e.target.value })}
                disabled={disabled}
                className="w-full bg-neutral-700 border border-neutral-600 rounded-lg px-3 py-2
                           focus:outline-none focus:border-amber-500 text-neutral-100"
              >
                {TIMEOUT_OPTIONS.map(opt => (
                  <option key={opt.value} value={opt.value}>{opt.label}</option>
                ))}
              </select>
            </td>
          </tr>

          {/* Display Mode */}
          <tr>
            <td className="text-neutral-300 pr-4 py-1 whitespace-nowrap align-middle">Display</td>
            <td className="py-1">
              <div className="flex gap-4">
                {([['stones', 'Stones'], ['xo', 'X and O']] as [DisplayMode, string][]).map(([mode, label]) => (
                  <label key={mode} className="flex items-center gap-2 cursor-pointer">
                    <input type="radio" name="displayMode" value={mode}
                      checked={settings.displayMode === mode}
                      onChange={() => update({ displayMode: mode })}
                      disabled={disabled}
                      className="accent-amber-500"
                    />
                    <span className="text-neutral-200">{label}</span>
                  </label>
                ))}
              </div>
            </td>
          </tr>

          {/* Undo */}
          <tr>
            <td className="text-neutral-300 pr-4 py-1 whitespace-nowrap align-middle">Undo</td>
            <td className="py-1">
              <label className="flex items-center gap-2 cursor-pointer">
                <input type="checkbox"
                  checked={settings.undoEnabled}
                  onChange={e => update({ undoEnabled: e.target.checked })}
                  disabled={disabled}
                  className="accent-amber-500 w-4 h-4"
                />
                <span className="text-neutral-400 text-sm">Allow undo moves</span>
              </label>
            </td>
          </tr>

          {/* Player Side - spanning header + options */}
          <tr>
            <td className="text-neutral-300 pr-4 pt-3 whitespace-nowrap align-top" rowSpan={2}>
              Your Side
            </td>
            <td className="pt-3 pb-0">
              <div className="grid grid-cols-2 gap-2 text-center">
                <span className="text-neutral-500 text-xs uppercase tracking-wide">First</span>
                <span className="text-neutral-500 text-xs uppercase tracking-wide">Second</span>
              </div>
            </td>
          </tr>
          <tr>
            <td className="pt-1 pb-1">
              <div className="grid grid-cols-2 gap-2">
                {(['X', 'O'] as PlayerSide[]).map(side => {
                  const label = isStones
                    ? (side === 'X' ? 'Black' : 'White')
                    : side
                  const active = settings.playerSide === side
                  return (
                    <button key={side}
                      onClick={() => !disabled && update({ playerSide: side })}
                      disabled={disabled}
                      className={`py-2 px-3 rounded-lg text-center font-medium transition-all
                        ${active
                          ? 'bg-amber-600 text-white shadow-md'
                          : 'bg-neutral-700 text-neutral-400 hover:bg-neutral-600 hover:text-neutral-200'
                        }
                        ${disabled ? 'opacity-50 cursor-not-allowed' : 'cursor-pointer'}
                      `}
                    >
                      {label}
                    </button>
                  )
                })}
              </div>
            </td>
          </tr>
        </tbody>
      </table>
    </div>
  )
}
