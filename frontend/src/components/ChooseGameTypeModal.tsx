import { useCallback, useEffect, useState } from 'react'
import ModalShell from './ModalShell'
import CopyableLinkRow from './CopyableLinkRow'
import {
  newGame as apiNewGame,
  cancelGame as apiCancelGame,
  type Color,
  type MultiplayerGameView,
} from '../lib/multiplayerClient'
import { useMultiplayerHostPolling } from '../hooks/useMultiplayerHostPolling'

type GameType = 'ai' | 'human'
type ColorChooser = 'host' | 'guest'

/**
 * Extract a 6-char Crockford-base32 code from raw user input. Accepts
 * either a bare code (`AB7K3X`) or a full URL containing `/play/<code>`.
 * Returns the uppercased code, or null if no valid code is present.
 */
export function extractInviteCode(raw: string): string | null {
  const trimmed = raw.trim()
  if (!trimmed) return null
  // Crockford-base32 alphabet (no I/L/O/U/0/1) — see api/app/multiplayer/codes.py.
  const ALPHA = '[2-9A-HJ-NP-Z]{6}'
  const urlMatch = trimmed.match(new RegExp(`/play/(${ALPHA})(?:[/?#].*)?$`, 'i'))
  if (urlMatch) return urlMatch[1].toUpperCase()
  const bareMatch = trimmed.match(new RegExp(`^${ALPHA}$`, 'i'))
  if (bareMatch) return bareMatch[0].toUpperCase()
  return null
}

interface Props {
  /** Auth token used to call the multiplayer API. */
  authToken: string
  /** User picked AI; let the host close us and start an AI game. */
  onAIChosen: () => void
  /**
   * Guest joined — navigate to /play/{code} (or whatever the multiplayer
   * page route is) so both players land in the same game UI.
   */
  onGuestJoined: (code: string) => void
  /**
   * Modal closed without a started multiplayer game. Caller should drop
   * the user into the AI flow per `doc/multiplayer-modal-plan.md` §1.
   */
  onClose: () => void
}

export default function ChooseGameTypeModal ({
  authToken,
  onAIChosen,
  onGuestJoined,
  onClose,
}: Props) {
  const [gameType, setGameType] = useState<GameType>('ai')
  const [chooser, setChooser] = useState<ColorChooser>('host')
  const [hostColor, setHostColor] = useState<Color>('X')
  const [creating, setCreating] = useState(false)
  const [createError, setCreateError] = useState<string | null>(null)
  const [game, setGame] = useState<MultiplayerGameView | null>(null)

  // Polling: only active once we have a created game.
  const { view: latest, secondsWaited, expired } = useMultiplayerHostPolling({
    token: authToken,
    code: game?.code ?? null,
  })

  // The actively-relevant view: prefer the latest poll, fall back to the
  // initial create response.
  const active = (latest as MultiplayerGameView | null) ?? game
  const inWaitingPhase = !!active && active.state === 'waiting'
  const cancelledRemotely = !!active && active.state === 'cancelled'

  // Side effects driven by the polled state — kept out of render to avoid
  // triggering parent setState during a child render pass.
  useEffect(() => {
    if (latest && latest.state === 'in_progress' && game) {
      onGuestJoined(game.code)
    }
  }, [latest, game, onGuestJoined])

  useEffect(() => {
    if (cancelledRemotely) onClose()
  }, [cancelledRemotely, onClose])

  // 15-minute hard cap — issue an explicit cancel POST so the row goes to
  // state='cancelled' immediately (don't rely on lazy expiry), then close.
  useEffect(() => {
    if (!expired || !game) return
    let cancelled = false
    void (async () => {
      try {
        await apiCancelGame(authToken, game.code)
      } catch {
        // Server-side lazy-expire path covers it eventually.
      }
      if (!cancelled) onClose()
    })()
    return () => {
      cancelled = true
    }
  }, [expired, game, authToken, onClose])

  // ----- Handlers --------------------------------------------------------

  const handleStart = useCallback(async () => {
    if (gameType === 'ai') {
      onAIChosen()
      return
    }
    setCreating(true)
    setCreateError(null)
    try {
      const created = await apiNewGame(authToken, {
        host_color: chooser === 'host' ? hostColor : null,
      })
      setGame(created)
    } catch (err) {
      setCreateError(err instanceof Error ? err.message : 'Could not create game')
    } finally {
      setCreating(false)
    }
  }, [gameType, chooser, hostColor, authToken, onAIChosen])

  const handleClose = useCallback(async () => {
    if (game && active && active.state === 'waiting') {
      try {
        await apiCancelGame(authToken, game.code)
      } catch {
        // Lazy-expire path covers it eventually.
      }
    }
    onClose()
  }, [game, active, authToken, onClose])

  // ----- Render ----------------------------------------------------------

  return (
    <ModalShell title='Choose Game Type' widthClassName='max-w-xl' onClose={handleClose}>
      <div className='space-y-6'>
        <fieldset>
          <legend className='sr-only'>Game type</legend>
          <div className='grid grid-cols-2 gap-3'>
            <RadioCard
              checked={gameType === 'ai'}
              onChange={() => setGameType('ai')}
              label='AI'
              hint='Default — play against the engine'
              disabled={inWaitingPhase || creating}
            />
            <RadioCard
              checked={gameType === 'human'}
              onChange={() => setGameType('human')}
              label='Another Player'
              hint='Send a link to a friend'
              disabled={inWaitingPhase || creating}
            />
          </div>
        </fieldset>

        {gameType === 'human' && !inWaitingPhase && (
          <>
            <fieldset>
              <legend className='mb-2 text-sm font-medium text-neutral-300'>
                Who chooses the playing color?
              </legend>
              <div className='grid grid-cols-2 gap-3'>
                <RadioCard
                  checked={chooser === 'host'}
                  onChange={() => setChooser('host')}
                  label='I will choose'
                  disabled={creating}
                />
                <RadioCard
                  checked={chooser === 'guest'}
                  onChange={() => setChooser('guest')}
                  label='Opponent will choose'
                  disabled={creating}
                />
              </div>
            </fieldset>

            {chooser === 'host' && (
              <fieldset>
                <legend className='mb-2 text-sm font-medium text-neutral-300'>
                  Your color
                </legend>
                <div className='grid grid-cols-2 gap-3'>
                  <RadioCard
                    checked={hostColor === 'X'}
                    onChange={() => setHostColor('X')}
                    label='Black (X)'
                    hint='Moves first'
                    disabled={creating}
                  />
                  <RadioCard
                    checked={hostColor === 'O'}
                    onChange={() => setHostColor('O')}
                    label='White (O)'
                    disabled={creating}
                  />
                </div>
              </fieldset>
            )}
          </>
        )}

        {createError && (
          <p className='rounded-md border border-red-500/50 bg-red-500/10 px-3 py-2 text-sm text-red-300'>
            {createError}
          </p>
        )}

        {!inWaitingPhase && (
          <div className='flex justify-end pt-2'>
            <button
              type='button'
              onClick={handleStart}
              disabled={creating}
              className='rounded-md bg-amber-500 px-5 py-2.5 text-sm font-bold text-neutral-900 shadow transition hover:bg-amber-400 disabled:cursor-wait disabled:opacity-60 focus:outline-none focus:ring-2 focus:ring-amber-300/50'
            >
              {creating ? 'Creating link…' : 'Start'}
            </button>
          </div>
        )}

        {inWaitingPhase && active && (
          <WaitingSection
            inviteUrl={active.invite_url}
            secondsWaited={secondsWaited}
          />
        )}

        {gameType === 'human' && (
          <JoinByCodeSection
            ownCode={game?.code ?? null}
            onJoin={async code => {
              // If the host is currently waiting on their own game, mark
              // it cancelled before pivoting to the opponent's game.
              if (game && active && active.state === 'waiting') {
                try {
                  await apiCancelGame(authToken, game.code)
                } catch {
                  // Lazy-expire path covers it eventually.
                }
              }
              onGuestJoined(code)
            }}
            disabled={creating}
          />
        )}
      </div>
    </ModalShell>
  )
}

// ===========================================================================
// Sub-components
// ===========================================================================

function RadioCard ({
  checked,
  onChange,
  label,
  hint,
  disabled,
}: {
  checked: boolean
  onChange: () => void
  label: string
  hint?: string
  disabled?: boolean
}) {
  return (
    <label
      className={`flex cursor-pointer items-start gap-3 rounded-lg border px-4 py-3 transition ${
        checked
          ? 'border-amber-400 bg-amber-500/10'
          : 'border-neutral-600 bg-neutral-700/40 hover:border-neutral-400'
      } ${disabled ? 'cursor-not-allowed opacity-50' : ''}`}
    >
      <input
        type='radio'
        checked={checked}
        onChange={onChange}
        disabled={disabled}
        className='mt-1 h-4 w-4 accent-amber-500'
      />
      <span className='flex-1'>
        <span className='block text-sm font-semibold text-white'>{label}</span>
        {hint && <span className='block text-xs text-neutral-400'>{hint}</span>}
      </span>
    </label>
  )
}

function JoinByCodeSection ({
  ownCode,
  onJoin,
  disabled,
}: {
  /** Caller's currently-hosted code, if any — used to reject self-paste. */
  ownCode: string | null
  onJoin: (code: string) => void | Promise<void>
  disabled?: boolean
}) {
  const [value, setValue] = useState('')
  const [error, setError] = useState<string | null>(null)

  function submit () {
    const code = extractInviteCode(value)
    if (!code) {
      setError('Enter a 6-character code or a /play/<CODE> link.')
      return
    }
    if (ownCode && code === ownCode) {
      setError("That's your own invitation — share it with your opponent.")
      return
    }
    setError(null)
    // Whoever's code is entered becomes the active game. The opponent
    // (the host of that code) stays the host; the caller joins as guest.
    void onJoin(code)
  }

  return (
    <div className='space-y-2 border-t border-neutral-700 pt-5'>
      <label className='block text-sm font-medium text-neutral-300'>
        Got an invitation? Paste the link or 6-character code:
      </label>
      <div className='flex items-stretch gap-2'>
        <input
          type='text'
          value={value}
          onChange={e => {
            setValue(e.target.value)
            setError(null)
          }}
          onKeyDown={e => {
            if (e.key === 'Enter') {
              e.preventDefault()
              submit()
            }
          }}
          placeholder='AB7K3X or https://…/play/AB7K3X'
          disabled={disabled}
          aria-label='Invitation code or link'
          className='min-w-0 flex-1 rounded-md border border-neutral-600 bg-neutral-900 px-3 py-2 font-mono text-sm text-neutral-100 placeholder:text-neutral-500 focus:outline-none focus:ring-2 focus:ring-amber-500/50 disabled:opacity-60'
        />
        <button
          type='button'
          onClick={submit}
          disabled={disabled || value.trim().length === 0}
          className='rounded-md border border-neutral-600 bg-neutral-700 px-4 py-2 text-sm font-semibold text-neutral-100 transition hover:bg-neutral-600 disabled:cursor-not-allowed disabled:opacity-50 focus:outline-none focus:ring-2 focus:ring-amber-500/50'
        >
          Join
        </button>
      </div>
      {error && <p className='text-xs text-red-400'>{error}</p>}
    </div>
  )
}

function WaitingSection ({
  inviteUrl,
  secondsWaited,
}: {
  inviteUrl: string
  secondsWaited: number
}) {
  const minutes = Math.floor(secondsWaited / 60)
  const seconds = secondsWaited % 60
  return (
    <div className='space-y-4 border-t border-neutral-700 pt-5'>
      <p className='text-base font-semibold text-amber-300'>
        Waiting for your opponent to join…
      </p>
      <p className='text-sm tabular-nums text-neutral-300'>
        Waiting time: {minutes} minutes, {seconds} seconds
      </p>
      <CopyableLinkRow url={inviteUrl} />
      <p className='text-xs leading-relaxed text-neutral-400'>
        This is your invitation link to the game you are hosting. Please send
        this link to your opponent. They must click on it within 15 minutes,
        or the link will expire. Once they click on it, they will join your
        game and this dialog box will disappear.
      </p>
    </div>
  )
}
