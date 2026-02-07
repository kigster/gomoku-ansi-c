import { useState } from 'react'

interface NameModalProps {
  onSubmit: (name: string) => void
}

export default function NameModal({ onSubmit }: NameModalProps) {
  const [name, setName] = useState('')

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault()
    if (name.trim()) {
      onSubmit(name.trim())
    }
  }

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm">
      <form
        onSubmit={handleSubmit}
        className="bg-neutral-800 rounded-2xl shadow-2xl p-8 max-w-md w-full mx-4"
      >
        <h2 className="text-2xl font-bold mb-2 text-center text-white">
          Welcome to <span className="font-heading text-amber-400">Gomoku</span>
        </h2>
        <p className="text-neutral-400 text-center mb-6" style={{ fontSize: '14pt' }}>
          What should we call you?
          <br />
          <span className="text-neutral-500">
            (just so we can address you properly &mdash; saved locally in your browser)
          </span>
        </p>
        <input
          type="text"
          value={name}
          onChange={e => setName(e.target.value)}
          placeholder="Your name"
          autoFocus
          className="w-full px-4 py-3 rounded-lg bg-neutral-700 border border-neutral-600
                     text-neutral-100 placeholder-neutral-500 focus:outline-none
                     focus:border-amber-500 focus:ring-1 focus:ring-amber-500 mb-4"
        />
        <button
          type="submit"
          disabled={!name.trim()}
          className="w-full py-3 rounded-lg font-semibold text-lg
                     bg-amber-600 hover:bg-amber-500 disabled:bg-neutral-600
                     disabled:text-neutral-400 transition-colors"
        >
          Continue
        </button>
      </form>
    </div>
  )
}
