import ModalShell from './ModalShell'

interface DifficultySettingsModalProps {
  onClose: () => void
}

export default function DifficultySettingsModal ({
  onClose
}: DifficultySettingsModalProps) {
  return (
    <ModalShell
      title={
        <>
          Difficulty <span className='text-amber-400'>Settings</span>
        </>
      }
      onClose={onClose}
      widthClassName='max-w-3xl'
      bodyClassName='space-y-5 text-neutral-300'
      footer={
        <button
          onClick={onClose}
          className='w-full rounded-xl bg-green-600 py-3 text-lg font-bold font-heading
                     shadow-lg shadow-green-900/30 transition-all cursor-pointer
                     hover:bg-green-500 active:bg-green-700'
        >
          Close
        </button>
      }
    >
      <p className='text-lg text-neutral-200'>
        This game exposes a couple of settings for you to control your AI
        opponent.
      </p>
      <div className='rounded-2xl border border-amber-500/20 bg-amber-500/8 p-5'>
        <p className='font-semibold text-amber-300'>AI Difficulty Settings</p>
        <p className='mt-2'>
          The following elements are in your control:
          <ul className='list-disc ml-4 mt-2 space-y-1'>
            <li>
              <strong>AI Depth</strong> — how far deep into the future the AI
              will search for the best move. Values of 5 and 6 generalluy
              produce a formidable opponent, but may take a bit longer. This is
              a part of the mini-max algorithm that AI uses.
            </li>
            <li>
              <strong>AI Radius</strong> — this determines how far from the
              existing placed stones should the AI search for potential best
              move. A Value of 2+ means it's actualluy looking around the
              current configuration. Increasing this also increases the search
              time.
            </li>
            <li>
              <strong>AI Timeout</strong> — this determines how long the AI is
              allowed to search for the best move. If you set it to 30 seconds,
              and AI is still looking for best move, at 30 seconds it will
              interrupt it's search and place the best move found so far. Use
              this only if AI is taking forever and you'd rather move along :)
            </li>
            <li>
              Who moves first. There is a strong first mover advantage so if you
              want to play on extra hard mode, play as the white player.
            </li>
            <li>
              Finally, you can choose whether you'd like to play using black and
              white stones, or X's and O's.
            </li>
          </ul>
        </p>
        <p className='font-semibold text-amber-300 mt-4'>Leaderboard Scoring</p>
        <p className='mt-2'>
          Anytime you win, your score is automatically computed and saved in the
          database. The top 100 global players who have the highest score on at
          least one game are shown on the leaderboard.
        </p>
        <p className='mt-2'>
          Therefore it would probably be a good idea to explain how the score is
          computed.
        </p>
        <ul className='list-disc ml-4 mt-2 space-y-1'>
          <li>
            The score rewards the games that you win quickly, and penalizes the
            games that you lose slowly.
          </li>
          <li>
            The score rewards when you win against more difficult AI settings
            (such as higher depth and radius)
          </li>
          <li>
            Finally, if you play the second turn player (white stone or a O) you
            get an additional bump in the final score because of the well known
            first-mover advantage.
          </li>
        </ul>
        <p className='mt-2'>
          NOTE: Lost games count as zero points and have no affect.
        </p>
        <p className='mt-2'>
          If you have any suggestions on how to improve the scoring system,
          please reach out to us at kig AT kig.re.
        </p>
      </div>
    </ModalShell>
  )
}
