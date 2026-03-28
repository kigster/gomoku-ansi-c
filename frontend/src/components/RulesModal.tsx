interface RulesModalProps {
  onClose: () => void
}

export default function RulesModal({ onClose }: RulesModalProps) {
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
            Gomoku <span className="text-amber-400">Rules</span>
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

        {/* Scrollable content */}
        <div className="flex-1 min-h-0 overflow-y-auto px-8 py-6 text-neutral-300 space-y-6 rules-content">
          <Section title="What is Gomoku?">
            <p>
              Gomoku (also known as <em>Five in a Row</em>, <em>Go-Moku</em>, or
              {' '}<em>Gobang</em>) is an abstract strategy board game traditionally
              played with black and white stones on a Go board. The standard board
              is 15&times;15, though 19&times;19 is also common. Players alternate
              placing stones on empty intersections, and the first to form an
              unbroken line of <strong>five stones</strong> &mdash; horizontally,
              vertically, or diagonally &mdash; wins.
            </p>
          </Section>

          <Section title="Basic Rules">
            <ol className="list-decimal space-y-2">
              <li><strong>Black plays first.</strong> Players alternate turns, placing one stone per turn.</li>
              <li>Stones are placed on <strong>intersections</strong> of the grid lines (not inside squares).</li>
              <li>Once placed, stones are <strong>never moved or removed</strong>.</li>
              <li>The first player to get <strong>exactly five in a row</strong> (horizontal, vertical, or diagonal) wins.</li>
              <li>If the board fills up with no five-in-a-row, the game is a <strong>draw</strong>.</li>
            </ol>
          </Section>

          <Section title="Game Variants">
            <h4 className="text-amber-400 font-semibold mt-2 mb-1">Freestyle Gomoku</h4>
            <p>
              The simplest variant. Five or more stones in a row wins. No restrictions
              on opening moves. This is what we play by default in this app.
            </p>

            <h4 className="text-amber-400 font-semibold mt-4 mb-1">Standard Gomoku</h4>
            <p>
              Same as freestyle, but a line of <em>exactly</em> five wins &mdash;
              an &ldquo;overline&rdquo; (six or more in a row) does <em>not</em> count
              as a win.
            </p>

            <h4 className="text-amber-400 font-semibold mt-4 mb-1">Renju</h4>
            <p>
              A tournament variant designed to offset Black&rsquo;s first-move advantage.
              Black is subject to three additional restrictions:
            </p>
            <ul className="list-disc space-y-1 mt-1">
              <li><strong>Double-three:</strong> Black cannot place a stone that simultaneously creates two open rows of three.</li>
              <li><strong>Double-four:</strong> Black cannot create two rows of four at the same time.</li>
              <li><strong>Overline:</strong> A line of six or more does not count as a win for Black.</li>
            </ul>
            <p className="mt-1">White has no such restrictions and can win with an overline.</p>

            <h4 className="text-amber-400 font-semibold mt-4 mb-1">Swap / Swap2 Opening</h4>
            <p>
              To further balance the game, some tournaments use opening rules where the
              first player places one or more stones and the opponent can choose to
              swap colors.
            </p>
          </Section>

          <Section title="The First-Mover Advantage">
            <p>
              In unrestricted Gomoku on a 15&times;15 board, Black (the first player)
              has a proven winning strategy. This was demonstrated computationally and
              is why tournament rules like Renju and swap openings exist &mdash; to
              make the game fair for both sides. This is also why in this
              version we default to human first player, but if you want
              additional challenge feel free to choose O or white stone.
            </p>
          </Section>

          <Section title="Strategy Tips">
            <h4 className="text-amber-400 font-semibold mb-2">Build Threats</h4>
            <p>
              The key to winning Gomoku is creating <strong>threats</strong> &mdash;
              configurations that force your opponent to respond defensively.
              The strongest threats are:
            </p>
            <ul className="list-disc space-y-1 mt-2">
              <li>
                <strong>Open Four:</strong> Four stones in a row with both ends empty.
                This is <em>unstoppable</em> &mdash; the opponent cannot block both sides.
              </li>
              <li>
                <strong>Four:</strong> Four in a row with one end blocked.
                Forces the opponent to block the open end immediately.
              </li>
              <li>
                <strong>Open Three:</strong> Three stones in a row with both ends open.
                If not blocked, becomes an open four on the next turn.
              </li>
            </ul>

            <h4 className="text-amber-400 font-semibold mt-4 mb-2">Threat Sequences</h4>
            <p>
              Advanced play revolves around <strong>threat sequences</strong> (also
              called <em>VCT</em> &mdash; Victory by Continuous Threats). The idea is
              to chain forcing moves: each move creates a threat the opponent
              <em> must</em> answer, and your next move creates another threat, until
              you reach an unstoppable position (like a double-four or four-three fork).
            </p>
            <p className="mt-2">
              A typical winning pattern: build an <strong>open three</strong>, force a
              block, then create a <strong>second open three</strong> elsewhere. The
              opponent can only block one, and the other becomes an open four.
            </p>

            <h4 className="text-amber-400 font-semibold mt-4 mb-2">General Tips</h4>
            <ul className="list-disc space-y-1">
              <li>Play near the <strong>center</strong> of the board early &mdash; it maximizes your options.</li>
              <li>Always check if your opponent has a <strong>four</strong> or <strong>open three</strong> before making an offensive move.</li>
              <li>Try to create <strong>double threats</strong> (fork positions) that cannot be blocked with a single move.</li>
              <li>Keep your stones <strong>connected</strong> &mdash; scattered stones are harder to form into winning lines.</li>
            </ul>
          </Section>

          <Section title="AI Settings in This App">
            <p>You can tune the AI opponent through the Settings panel:</p>
            <ul className="list-disc space-y-2 mt-2">
              <li>
                <strong>Search Depth (2&ndash;5):</strong> How many moves ahead the AI
                looks. Higher values produce stronger play but take longer to compute.
              </li>
              <li>
                <strong>Search Radius (1&ndash;4):</strong> How far from existing stones
                the AI considers placing new ones. Larger radius explores more of the
                board but is slower.
              </li>
              <li>
                <strong>Timeout:</strong> Maximum time the AI spends on a single move.
                Useful for keeping the game flowing.
              </li>
              <li>
                <strong>Board Size (15 or 19):</strong> The dimension of the playing grid.
              </li>
            </ul>
            <p className="mt-2 text-neutral-400 text-sm">
              The AI uses minimax search with alpha-beta pruning, threat detection,
              transposition tables, and iterative deepening for optimal performance
              within the time and depth limits you set.
            </p>
          </Section>

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

function Section({ title, children }: { title: string; children: React.ReactNode }) {
  return (
    <section>
      <h3 className="text-lg font-bold text-white mb-2">{title}</h3>
      {children}
    </section>
  )
}
