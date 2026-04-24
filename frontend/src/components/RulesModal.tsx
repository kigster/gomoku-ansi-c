import { createPortal } from 'react-dom'

interface RulesModalProps {
  onClose: () => void
}

export default function RulesModal({ onClose }: RulesModalProps) {
  return createPortal(
    <div className="fixed inset-0 z-[100000] flex items-center justify-center">
      <div className="absolute inset-0 bg-black/60 backdrop-blur-sm" onClick={onClose} />
      <div className="relative bg-neutral-900 border border-neutral-700 rounded-xl shadow-2xl
                      w-full max-w-2xl max-h-[85vh] mx-4 flex flex-col">
        <div className="flex items-center justify-between px-6 py-4 border-b border-neutral-700">
          <h3 className="font-heading text-2xl font-bold text-amber-400">Gomoku Rules</h3>
          <button
            onClick={onClose}
            className="text-neutral-400 hover:text-neutral-200 text-2xl leading-none cursor-pointer"
            aria-label="Close"
          >
            &times;
          </button>
        </div>

        <div className="overflow-y-auto px-6 py-5 text-neutral-300 space-y-4 leading-relaxed">
          <p>
            <span className="text-amber-300 font-semibold">Gomoku</span> (五目並べ,
            "five in a row") is a two-player abstract strategy game. Players take turns
            placing stones on the intersections of a 15x15 or 19x19 grid. The first to
            line up <span className="text-amber-300 font-semibold">exactly five</span> of
            their stones in an unbroken row — horizontally, vertically, or diagonally — wins.
          </p>

          <p>
            <span className="text-amber-300 font-semibold">X moves first.</span> Since
            the first player has a known advantage on an empty board, tournament play
            uses one of several restrictive rule sets that level the field for Black.
          </p>

          <h4 className="font-heading text-lg font-bold text-amber-400 pt-2">Variants</h4>
          <ul className="list-disc list-inside space-y-2 marker:text-amber-500/70">
            <li>
              <span className="text-neutral-200 font-semibold">Standard / Street</span> —
              the version this app uses by default. Five in a row wins; six or more
              (an "overline") does not count.
            </li>
            <li>
              <span className="text-neutral-200 font-semibold">Freestyle</span> —
              five or more in a row wins, including overlines.
            </li>
            <li>
              <span className="text-neutral-200 font-semibold">Renju</span> — Black is
              forbidden from creating double-threes, double-fours, or overlines. White
              has no restrictions. Used in professional tournament play.
            </li>
            <li>
              <span className="text-neutral-200 font-semibold">Caro</span> — five in a
              row wins only if at least one end of the row is open.
            </li>
            <li>
              <span className="text-neutral-200 font-semibold">Omok</span> — Korean
              variant; both sides forbidden from making double-threes.
            </li>
          </ul>

          <p className="text-neutral-400 text-sm">
            For the full reference covering all variants and the four modern Renju
            opening procedures (RIF, Yamaguchi, Soosyrv-8, Taraguchi-10), see{' '}
            <a
              href="https://github.com/kigster/gomoku-ansi-c/blob/main/doc/GOMOKU-RENJU-RULES.md"
              target="_blank"
              rel="noopener noreferrer"
              className="text-amber-400 hover:text-amber-300 underline"
            >
              doc/GOMOKU-RENJU-RULES.md
            </a>
            .
          </p>

          <h4 className="font-heading text-lg font-bold text-amber-400 pt-2">AI Settings</h4>
          <ul className="list-disc list-inside space-y-2 marker:text-amber-500/70">
            <li>
              <span className="text-neutral-200 font-semibold">AI Depth</span> — how
              many plies the engine looks ahead (2–5). Higher is stronger and slower.
            </li>
            <li>
              <span className="text-neutral-200 font-semibold">AI Radius</span> — how
              far from existing stones the engine considers candidate moves (1–4).
            </li>
            <li>
              <span className="text-neutral-200 font-semibold">AI Timeout</span> — wall
              clock budget per move. The engine returns its best move so far when the
              budget runs out.
            </li>
          </ul>
        </div>
      </div>
    </div>,
    document.body,
  )
}
