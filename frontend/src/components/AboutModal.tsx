interface AboutModalProps {
  onClose: () => void
}

export default function AboutModal({ onClose }: AboutModalProps) {
  return (
    <div
      className="fixed inset-0 z-50 flex items-start sm:items-center justify-center pt-[5vh] sm:pt-0 bg-black/60 backdrop-blur-sm"
      onClick={onClose}
    >
      <div
        className="bg-neutral-800 rounded-2xl shadow-2xl max-w-5xl w-[95%] max-h-[80vh] overflow-y-auto relative"
        onClick={e => e.stopPropagation()}
      >
        {/* Header */}
        <div className="flex items-center justify-between px-8 pt-6 pb-4 border-b border-neutral-700">
          <h2 className="text-2xl font-bold text-white font-heading">
            About <span className="text-amber-400">Gomoku</span>
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

        {/* Content */}
        <div className="px-8 py-6 text-neutral-300 space-y-5">
          <p className="text-lg">
            <span className="text-amber-400 font-heading font-bold">Gomoku</span> is
            a networked five-in-a-row game with a C-powered AI backend and a React
            frontend, running on Google Cloud Run infrastructure.
          </p>

          <section>
            <h3 className="text-white font-bold mb-2">History</h3>
            <p>
              Konstantin Gredeskoul started writing this game around <strong>2010</strong>.
              The original game engine featured a single but carefully tuned board
              evaluation function &mdash; no minimax, just a heuristic scorer that could
              still play a surprisingly decent game.
            </p>
            <p className="mt-2">
              Since then, the engine has evolved significantly: <strong>minimax with
              alpha-beta pruning</strong> (a classic and powerful algorithm from game
              theory) was added, along with a host of optimizations including early
              search exit, transposition/caching tables, iterative deepening, and
              threat-sequence detection. The result is an AI opponent that plays at
              a strong level while remaining responsive.
            </p>
          </section>

          <section>
            <h3 className="text-white font-bold mb-2">Technology</h3>
            <ul className="list-disc list-inside space-y-1 ml-2">
              <li>Game engine: <strong>ANSI C</strong> with an HTTP server layer</li>
              <li>Frontend: <strong>React + TypeScript + Vite + TailwindCSS</strong></li>
              <li>Infrastructure: <strong>Google Cloud Run</strong> with Docker containers</li>
            </ul>
          </section>

          <section>
            <h3 className="text-white font-bold mb-2">Links</h3>
            <div className="flex flex-wrap gap-4">
              <a
                href="https://kig.re/"
                target="_blank"
                rel="noopener noreferrer"
                className="text-amber-400 hover:text-amber-300 underline underline-offset-2
                           transition-colors"
              >
                kig.re
              </a>
              <a
                href="https://reinvent.one"
                target="_blank"
                rel="noopener noreferrer"
                className="text-amber-400 hover:text-amber-300 underline underline-offset-2
                           transition-colors"
              >
                reinvent.one
              </a>
              <a
                href="https://github.com/kigster/gomoku-ansi-c"
                target="_blank"
                rel="noopener noreferrer"
                className="text-amber-400 hover:text-amber-300 underline underline-offset-2
                           transition-colors"
              >
                GitHub
              </a>
            </div>
          </section>

          <footer className="pt-3 border-t border-neutral-700 text-neutral-400 text-sm">
            &copy; 2026{' '}
            <a
              href="https://kig.re/"
              target="_blank"
              rel="noopener noreferrer"
              className="text-neutral-300 hover:text-amber-400 transition-colors"
            >
              Konstantin Gredeskoul
            </a>
            . Co-authored by{' '}
            <span className="text-neutral-300">Claude AI</span>.
            All rights reserved.
          </footer>

          <button
            onClick={onClose}
            className="w-full mt-6 py-3 rounded-xl text-lg font-bold font-heading
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
