import { useState, useCallback } from 'react'
import { createPortal } from 'react-dom'
import { Prism as SyntaxHighlighter } from 'react-syntax-highlighter'
import {
  oneDark,
  dracula,
  vscDarkPlus,
  nightOwl,
  nord,
  synthwave84,
  atomDark,
  darcula,
  gruvboxDark,
  materialOceanic,
  okaidia,
  solarizedDarkAtom,
} from 'react-syntax-highlighter/dist/esm/styles/prism'
import { getLastExchange } from '../api'

const THEMES: Record<string, { label: string; style: Record<string, React.CSSProperties> }> = {
  oneDark:            { label: 'One Dark',          style: oneDark },
  dracula:            { label: 'Dracula',           style: dracula },
  vscDarkPlus:        { label: 'VS Code Dark+',    style: vscDarkPlus },
  nightOwl:           { label: 'Night Owl',         style: nightOwl },
  nord:               { label: 'Nord',              style: nord },
  synthwave84:        { label: 'Synthwave 84',      style: synthwave84 },
  atomDark:           { label: 'Atom Dark',         style: atomDark },
  darcula:            { label: 'Darcula',           style: darcula },
  gruvboxDark:        { label: 'Gruvbox Dark',      style: gruvboxDark },
  materialOceanic:    { label: 'Material Oceanic',  style: materialOceanic },
  okaidia:            { label: 'Okaidia',           style: okaidia },
  solarizedDarkAtom:  { label: 'Solarized Dark',   style: solarizedDarkAtom },
}

const DEFAULT_THEME = 'oneDark'

interface JsonDebugModalProps {
  className?: string
}

export default function JsonDebugModal({ className }: JsonDebugModalProps) {
  const [open, setOpen] = useState(false)
  const [tab, setTab] = useState<'request' | 'response'>('request')
  const [themeKey, setThemeKey] = useState(DEFAULT_THEME)

  const exchange = getLastExchange()

  const handleOpen = useCallback(() => setOpen(true), [])
  const handleClose = useCallback(() => setOpen(false), [])
  const handleThemeChange = useCallback((e: React.ChangeEvent<HTMLSelectElement>) => {
    setThemeKey(e.target.value)
  }, [])

  const activeTheme = THEMES[themeKey] ?? THEMES.oneDark

  return (
    <>
      <button
        onClick={handleOpen}
        title="View last API exchange"
        className={className ?? "text-neutral-400 hover:text-neutral-200 transition-colors cursor-pointer px-2 py-1"}
      >
        Game as JSON
      </button>

      {open && createPortal(
        <div className="fixed inset-0 z-[100000] flex items-start sm:items-center justify-center pt-[5vh] sm:pt-0">
          <div className="absolute inset-0 bg-black/60 backdrop-blur-sm" onClick={handleClose} />
          <div className="relative bg-neutral-900 border border-neutral-700 rounded-2xl shadow-2xl
                          w-[95%] max-w-5xl max-h-[80vh] flex flex-col">
            {/* Header */}
            <div className="flex items-center justify-between px-5 py-3 border-b border-neutral-700">
              <h3 className="font-heading text-lg font-bold text-amber-400">API Debug</h3>
              <div className="flex items-center gap-3">
                <select
                  value={themeKey}
                  onChange={handleThemeChange}
                  className="bg-neutral-800 border border-neutral-600 text-neutral-300 text-xs
                             rounded px-2 py-1 cursor-pointer focus:outline-none focus:border-amber-500"
                >
                  {Object.entries(THEMES).map(([key, { label }]) => (
                    <option key={key} value={key}>{label}</option>
                  ))}
                </select>
                <button onClick={handleClose}
                        className="text-neutral-400 hover:text-neutral-200 text-xl leading-none cursor-pointer">
                  &times;
                </button>
              </div>
            </div>

            {!exchange ? (
              <div className="p-6 text-neutral-500 text-center">
                No API calls have been made yet.
              </div>
            ) : (
              <>
                {/* Tabs */}
                <div className="flex border-b border-neutral-700">
                  <button
                    onClick={() => setTab('request')}
                    className={`flex-1 py-2 text-sm font-medium transition-colors cursor-pointer
                      ${tab === 'request'
                        ? 'text-amber-400 border-b-2 border-amber-400'
                        : 'text-neutral-400 hover:text-neutral-200'}`}
                  >
                    Request
                  </button>
                  <button
                    onClick={() => setTab('response')}
                    className={`flex-1 py-2 text-sm font-medium transition-colors cursor-pointer
                      ${tab === 'response'
                        ? 'text-amber-400 border-b-2 border-amber-400'
                        : 'text-neutral-400 hover:text-neutral-200'}`}
                  >
                    Response
                  </button>
                </div>

                {/* Body */}
                <div className="flex-1 overflow-auto">
                  {exchange.error && tab === 'response' && (
                    <div className="mx-4 mt-3 px-3 py-2 bg-red-900/30 border border-red-700/50 rounded text-red-400 text-sm">
                      {exchange.error}
                    </div>
                  )}
                  <SyntaxHighlighter
                    language="json"
                    style={activeTheme.style}
                    customStyle={{ margin: 0, borderRadius: 0, fontSize: '12px' }}
                    wrapLongLines
                  >
                    {JSON.stringify(
                      tab === 'request' ? exchange.request : (exchange.response ?? null),
                      null,
                      2,
                    )}
                  </SyntaxHighlighter>
                </div>

                {/* Timestamp */}
                <div className="px-5 py-2 border-t border-neutral-700">
                  <span className="text-xs text-neutral-500">
                    {new Date(exchange.timestamp).toLocaleTimeString()}
                  </span>
                </div>
              </>
            )}
            <div className="flex gap-3 px-5 pb-4 pt-2">
              {exchange && (
                <button
                  onClick={() => {
                    const data = exchange.response ?? exchange.request
                    const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' })
                    const url = URL.createObjectURL(blob)
                    const a = document.createElement('a')
                    a.href = url
                    a.download = `gomoku-game-${new Date().toISOString().slice(0, 19).replace(/:/g, '-')}.json`
                    a.click()
                    URL.revokeObjectURL(url)
                  }}
                  className="flex-1 py-3 rounded-xl text-lg font-bold font-heading
                             bg-amber-500 hover:bg-amber-400 active:bg-amber-600
                             shadow-lg shadow-amber-900/30 transition-all cursor-pointer"
                >
                  Download JSON
                </button>
              )}
              <button
                onClick={handleClose}
                className="flex-1 py-3 rounded-xl text-lg font-bold font-heading
                           bg-green-600 hover:bg-green-500 active:bg-green-700
                           shadow-lg shadow-green-900/30 transition-all cursor-pointer"
              >
                Close
              </button>
            </div>
          </div>
        </div>,
        document.body,
      )}
    </>
  )
}
