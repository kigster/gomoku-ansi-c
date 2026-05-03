import { useEffect, useMemo, useRef, useState } from 'react'

interface ChatMessage {
  id: string
  // Username of the speaker. `me` flags the active user's own messages so
  // the bubble is right-coloured (white-on-blue) and left-anchored per
  // the design spec.
  speaker: string
  me: boolean
  body: string
  // Local timestamp for grouping / display. ISO so it survives JSON.
  at: string
  // True for system-generated lines like "/invite sent to @bob". Rendered
  // as a centred caption rather than a chat bubble.
  system?: boolean
  // "info" (blue) | "error" (red) — colour of the system caption.
  systemKind?: 'info' | 'error'
}

interface ChatPanelProps {
  // Currently authed username — used to render bubbles "as me".
  meUsername: string
  // Username of the person on the other end. `null` when there's no
  // active conversation yet (we then show a friendly placeholder header
  // asking the user to /invite someone).
  peerUsername: string | null
  // Auth token; passed to the slash-command endpoints.
  authToken: string
  // Backend base URL.
  apiBase: string
  // Fires when a slash command terminated the active multiplayer game.
  // Today only `/block` triggers this — `/unfollow` is intentionally a
  // pure social-graph operation that does NOT cascade into game
  // termination (matches the server-side contract; see
  // app/routers/social.py module docstring).
  onActiveGameTerminated?: () => void
  // 'dark' (default) — right-rail panel on the dark home page.
  // 'light' — in-game panel embedded in the multiplayer board view.
  //          White card, blue-on-white own bubbles right, gray-on-white
  //          peer bubbles left, per the spec.
  variant?: 'dark' | 'light'
  // 'card' (default) — fixed height card with internal scroll, suitable
  // for the right rail. 'fill' — stretch to the parent's full height,
  // used when the chat panel replaces the full left column during a
  // multiplayer game.
  height?: 'card' | 'fill'
}

// Slash commands the chat panel understands. Each one captures the target
// username (no leading `@`) and dispatches to a different REST endpoint.
//
// /invite   @user → POST /chat/invite      (game invite, presence-aware,
//                                            rate-limited per caller on a
//                                            rolling window: 7/hour, 15/24h.
//                                            429 detail is structured —
//                                            see formatErrorDetail below)
// /block    @user → POST /social/block     (hard block; if the two are in
//                                            an active game it terminates
//                                            immediately and the UI returns
//                                            to the standard idle view)
// /follow   @user → POST /social/follow    (one-directional; mutual = friend)
// /unfollow @user → POST /social/unfollow  (drops the follow; idempotent;
//                                            does NOT terminate any active
//                                            game — only /block does that)
//
// The followee can send invites to anyone who follows them even when the
// follow isn't reciprocal — only mutual follows count as "friends".
const SLASH_USERNAME = '([\\wÀ-ɏ0-9\\-\\^]{2,30})'
const SLASH_RE: Record<SlashAction, RegExp> = {
  invite: new RegExp(`^\\s*/invite\\s+@?${SLASH_USERNAME}\\s*$`, 'i'),
  block: new RegExp(`^\\s*/block\\s+@?${SLASH_USERNAME}\\s*$`, 'i'),
  follow: new RegExp(`^\\s*/follow\\s+@?${SLASH_USERNAME}\\s*$`, 'i'),
  unfollow: new RegExp(`^\\s*/unfollow\\s+@?${SLASH_USERNAME}\\s*$`, 'i'),
}
// `/help` takes no argument; matched separately so the user gets the
// command list without typing a target username.
const HELP_RE = /^\s*\/help\s*$/i

// One short line per command, joined with newlines for a clean monospaced
// help overlay in the chat. Kept terse — the chat panel is narrow.
const HELP_TEXT = [
  '/invite @user   — invite the user to a game',
  '/follow @user   — follow them (mutual = friends)',
  '/unfollow @user — drop the follow (does not end any game)',
  '/block @user    — block them (ends an active game)',
  '/help           — this list',
].join('\n')

type SlashAction = 'invite' | 'block' | 'follow' | 'unfollow'

interface SlashSpec {
  endpoint: string
  // Past-tense success caption for the system message.
  successCaption: (target: string, body: SlashResponseBody) => string
  errorCaption: (target: string, msg: string) => string
}

interface SlashResponseBody {
  delivered?: boolean
  target_state?: 'in_game' | 'idle' | 'offline'
  reciprocal?: boolean
  // True iff `/block` terminated an active multiplayer game between the
  // two. The chat panel surfaces this in the system caption AND fires
  // onActiveGameTerminated upstream so App.tsx drops the user back to
  // the idle view. /unfollow no longer touches games (the field is
  // never set on its response).
  game_terminated?: boolean
  // /unfollow always returns this; included here so the SlashResponseBody
  // type covers all four endpoints' shapes.
  unfollowed?: boolean
}

const SLASH_SPECS: Record<SlashAction, SlashSpec> = {
  invite: {
    endpoint: '/chat/invite',
    successCaption: (target, body) => {
      const where =
        body.target_state === 'in_game'
          ? 'in a game'
          : body.target_state === 'idle'
            ? 'online'
            : 'offline'
      return `Invite sent to @${target} (${where}).`
    },
    errorCaption: (target, msg) => `Could not invite @${target}: ${msg}`,
  },
  block: {
    endpoint: '/social/block',
    successCaption: (target, body) =>
      body.game_terminated
        ? `Blocked @${target}. The game with them was ended.`
        : `Blocked @${target}. They won't be able to chat with you or invite you to a game.`,
    errorCaption: (target, msg) => `Could not block @${target}: ${msg}`,
  },
  follow: {
    endpoint: '/social/follow',
    // Reciprocal flag from the server tells us whether the follow makes
    // the pair into mutual friends — surfaced in the success caption so
    // the user understands the directional model.
    successCaption: (target, body) =>
      body.reciprocal
        ? `Now friends with @${target} (you both follow each other).`
        : `Following @${target}. They can invite you to games; ask them to /follow you back to be friends.`,
    errorCaption: (target, msg) => `Could not follow @${target}: ${msg}`,
  },
  unfollow: {
    endpoint: '/social/unfollow',
    // No game_terminated branch — unfollow is a pure social-graph
    // operation (see app/routers/social.py). If the user wants to end
    // a game with someone, /block is the explicit verb.
    successCaption: target => `Unfollowed @${target}.`,
    errorCaption: (target, msg) => `Could not unfollow @${target}: ${msg}`,
  },
}

// FastAPI returns errors as `{detail: string | object}`. The /chat/invite
// 429 returns a structured detail `{error, retry_at}`; render it as a
// human sentence with a locale-formatted retry time. All other endpoints
// return a string detail and we pass it through unchanged.
async function formatErrorDetail (resp: Response): Promise<string> {
  const raw = await resp.text().catch(() => '')
  if (!raw) return `HTTP ${resp.status}`
  try {
    const parsed = JSON.parse(raw) as { detail?: unknown }
    const detail = parsed.detail
    if (typeof detail === 'string') return detail
    if (
      detail !== null &&
      typeof detail === 'object' &&
      'error' in detail &&
      typeof (detail as { error: unknown }).error === 'string'
    ) {
      const obj = detail as { error: string; retry_at?: string }
      if (obj.retry_at) {
        const when = new Date(obj.retry_at)
        if (!Number.isNaN(when.getTime())) {
          return `${obj.error} Try again at ${when.toLocaleTimeString()}.`
        }
      }
      return obj.error
    }
  } catch {
    // Body wasn't JSON — fall through and return it as-is.
  }
  return raw
}

export default function ChatPanel ({
  meUsername,
  peerUsername,
  authToken,
  apiBase,
  onActiveGameTerminated,
  variant = 'dark',
  height = 'card',
}: ChatPanelProps) {
  const isLight = variant === 'light'
  const sizeClass = height === 'fill'
    ? 'h-full min-h-[20rem]'
    : 'h-[22rem] lg:h-[26rem]'
  const [draft, setDraft] = useState('')
  const [messages, setMessages] = useState<ChatMessage[]>([])
  const [pending, setPending] = useState(false)
  const messagesRef = useRef<HTMLDivElement | null>(null)

  // Always scroll the chat to the bottom on new message — matches the
  // dominant chat-app convention (latest content visible by default).
  useEffect(() => {
    const node = messagesRef.current
    if (!node) return
    node.scrollTop = node.scrollHeight
  }, [messages])

  function pushMessage (m: Omit<ChatMessage, 'id' | 'at'>) {
    setMessages(prev => [
      ...prev,
      { ...m, id: crypto.randomUUID(), at: new Date().toISOString() },
    ])
  }

  async function dispatchSlash (action: SlashAction, target: string) {
    const spec = SLASH_SPECS[action]
    setPending(true)
    try {
      const resp = await fetch(`${apiBase}${spec.endpoint}`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          Authorization: `Bearer ${authToken}`,
        },
        body: JSON.stringify({ target_username: target }),
      })
      if (!resp.ok) {
        throw new Error(await formatErrorDetail(resp))
      }
      const body = (await resp.json().catch(() => ({}))) as SlashResponseBody
      pushMessage({
        speaker: 'system',
        me: false,
        body: spec.successCaption(target, body),
        system: true,
        systemKind: 'info',
      })
      if (body.game_terminated) onActiveGameTerminated?.()
    } catch (err) {
      const msg = err instanceof Error ? err.message : 'unknown error'
      pushMessage({
        speaker: 'system',
        me: false,
        body: spec.errorCaption(target, msg),
        system: true,
        systemKind: 'error',
      })
    } finally {
      setPending(false)
    }
  }

  function handleSubmit (e: React.FormEvent) {
    e.preventDefault()
    const text = draft.trim()
    if (!text) return
    setDraft('')

    // /help is local-only — no server round-trip.
    if (HELP_RE.test(text)) {
      pushMessage({ speaker: meUsername, me: true, body: '/help' })
      pushMessage({
        speaker: 'system',
        me: false,
        body: HELP_TEXT,
        system: true,
        systemKind: 'info',
      })
      return
    }

    // Try each slash command in turn. Each regex captures the target
    // username in group 1; on a match we echo the literal command into
    // the chat (so the user sees what they sent) and dispatch.
    for (const action of Object.keys(SLASH_RE) as SlashAction[]) {
      const m = SLASH_RE[action].exec(text)
      if (!m) continue
      const target = m[1]
      pushMessage({
        speaker: meUsername,
        me: true,
        body: `/${action} @${target}`,
      })
      void dispatchSlash(action, target)
      return
    }

    pushMessage({ speaker: meUsername, me: true, body: text })
    // When chat persistence ships, also POST this to the chat-messages
    // endpoint. For now, draft-only (echo into the local list so the UI
    // is testable end-to-end).
  }

  // Header label: yellow @username, or a friendlier placeholder when nobody
  // is on the other end yet.
  const peerLabel = useMemo(() => {
    if (peerUsername) return `@${peerUsername}`
    return 'No conversation'
  }, [peerUsername])

  // Tailwind classes per variant. Kept inline (vs CVA / className helper)
  // because there are only two variants and the diff between them is small
  // enough that a single colour palette per role is the clearest read.
  const styles = isLight
    ? {
        shell: 'bg-white border-neutral-300 shadow-sm',
        header: 'bg-neutral-50 border-b border-neutral-200',
        headerEyebrow: 'text-neutral-500',
        headerName: peerUsername ? 'text-blue-700' : 'text-neutral-400',
        messages: 'scrollbar-thin scrollbar-thumb-neutral-300',
        emptyState: 'text-neutral-500',
        emptyAccent: 'text-blue-700',
        inputRow: 'bg-neutral-50 border-t border-neutral-200',
        input:
          'bg-white border border-neutral-300 text-neutral-900 placeholder:text-neutral-400 ' +
          'focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-300/60',
        button:
          'bg-blue-600 text-white hover:bg-blue-500 ' +
          'disabled:cursor-not-allowed disabled:opacity-50 ' +
          'focus:outline-none focus:ring-2 focus:ring-blue-300/60',
      }
    : {
        shell: 'bg-neutral-900/70 border-neutral-700/80 shadow-inner shadow-black/30',
        header: 'bg-neutral-950 border-b border-neutral-800',
        headerEyebrow: 'text-neutral-500',
        headerName: peerUsername ? 'text-amber-300' : 'text-neutral-500',
        messages: 'scrollbar-thin scrollbar-thumb-neutral-700',
        emptyState: 'text-neutral-500',
        emptyAccent: 'text-amber-300/90',
        inputRow: 'bg-neutral-900 border-t border-neutral-800',
        input:
          'bg-neutral-800 border border-neutral-700 text-neutral-100 placeholder:text-neutral-500 ' +
          'focus:outline-none focus:border-amber-500/70 focus:ring-1 focus:ring-amber-500/40',
        button:
          'bg-amber-600 text-neutral-900 hover:bg-amber-500 ' +
          'disabled:cursor-not-allowed disabled:opacity-50 ' +
          'focus:outline-none focus:ring-2 focus:ring-amber-300/50',
      }

  return (
    // Fixed-height column when used as the right-rail card; stretch-to-fill
    // when it replaces the entire left column during an in-game session.
    <div
      className={[
        'flex flex-col rounded-xl border overflow-hidden',
        sizeClass,
        styles.shell,
      ].join(' ')}
    >
      {/* Header */}
      <header
        className={[
          'flex items-center justify-between gap-3 px-4 py-2.5',
          styles.header,
        ].join(' ')}
      >
        <div className='flex flex-col min-w-0'>
          <span
            className={[
              'text-[10px] uppercase tracking-[0.18em] font-semibold',
              styles.headerEyebrow,
            ].join(' ')}
          >
            Chat with
          </span>
          <span
            className={[
              'truncate font-heading text-base font-semibold',
              styles.headerName,
            ].join(' ')}
          >
            {peerLabel}
          </span>
        </div>
        <PresenceDot connected={!!peerUsername} />
      </header>

      {/* Messages — scrolls. */}
      <div
        ref={messagesRef}
        className={[
          'flex-1 min-h-0 overflow-y-auto px-3 py-3 space-y-2',
          styles.messages,
        ].join(' ')}
      >
        {messages.length === 0 && (
          <p className={['text-center text-xs italic mt-4', styles.emptyState].join(' ')}>
            No messages yet. Say hi, type{' '}
            <code className={['font-mono', styles.emptyAccent].join(' ')}>
              /invite @username
            </code>{' '}
            to start a multiplayer game, or{' '}
            <code className={['font-mono', styles.emptyAccent].join(' ')}>/help</code>{' '}
            for the command list.
          </p>
        )}
        {messages.map(m => (
          <Message key={m.id} m={m} variant={variant} />
        ))}
      </div>

      {/* Input row. */}
      <form
        onSubmit={handleSubmit}
        className={['flex items-stretch gap-2 px-3 py-2.5', styles.inputRow].join(' ')}
      >
        <input
          type='text'
          value={draft}
          onChange={e => setDraft(e.target.value)}
          placeholder={
            peerUsername
              ? `Message @${peerUsername} or /invite @username…`
              : '/invite @username to start a game'
          }
          disabled={pending}
          aria-label='Chat message'
          className={[
            'flex-1 min-w-0 rounded-lg px-3 py-2 text-sm disabled:opacity-60',
            styles.input,
          ].join(' ')}
        />
        <button
          type='submit'
          disabled={pending || draft.trim().length === 0}
          className={[
            'rounded-lg px-4 py-2 text-sm font-semibold font-heading transition-colors',
            styles.button,
          ].join(' ')}
        >
          Send
        </button>
      </form>
    </div>
  )
}

function Message ({ m, variant }: { m: ChatMessage; variant: 'dark' | 'light' }) {
  const isLight = variant === 'light'
  if (m.system) {
    // Multi-line system output (e.g. /help) is rendered in a monospaced
    // block so the column-aligned command list stays legible. Single-line
    // captions still look like the centred italic notes used elsewhere.
    const isMultiline = m.body.includes('\n')
    const bgBlock = isLight
      ? 'bg-neutral-100 border border-neutral-200'
      : 'bg-neutral-800/60 border border-neutral-700/60'
    const errColor = isLight ? 'text-red-700' : 'text-red-400'
    const infoColor = isLight ? 'text-blue-700' : 'text-sky-300'
    return (
      <pre
        className={[
          'whitespace-pre-wrap text-xs px-3 py-2 rounded-md mx-2',
          isMultiline
            ? `font-mono text-left ${bgBlock}`
            : 'italic text-center font-sans bg-transparent border-0',
          m.systemKind === 'error' ? errColor : infoColor,
        ].join(' ')}
      >
        {m.body}
      </pre>
    )
  }
  // Convention follows iMessage / WhatsApp / Slack / Discord / Telegram:
  // the active user's bubbles sit on the RIGHT (white-on-blue), peer
  // bubbles on the LEFT (neutral). The asymmetric corner radius
  // ("tail" pointing toward the speaker's side) reinforces the side
  // mapping at a glance.
  const isMe = m.me
  const speakerColor = isLight ? 'text-neutral-500' : 'text-neutral-500'
  const meBubble = isLight
    ? 'rounded-tr-sm bg-blue-600 text-white'
    : 'rounded-tr-sm bg-blue-600 text-white'
  const peerBubble = isLight
    ? 'rounded-tl-sm bg-neutral-200 text-neutral-900 border border-neutral-300'
    : 'rounded-tl-sm bg-neutral-800 text-neutral-100 border border-neutral-700/60'
  const shadow = isLight ? 'shadow-sm shadow-neutral-300/40' : 'shadow-sm shadow-black/40'
  return (
    <div
      className={[
        'flex flex-col max-w-[88%]',
        isMe ? 'items-end ml-auto' : 'items-start mr-auto',
      ].join(' ')}
    >
      <span
        className={[
          'text-[10px] uppercase tracking-[0.14em] mb-0.5',
          speakerColor,
          isMe ? 'mr-2' : 'ml-2',
        ].join(' ')}
      >
        @{m.speaker}
      </span>
      <div
        className={[
          'rounded-2xl px-3.5 py-1.5 text-sm leading-snug',
          shadow,
          isMe ? meBubble : peerBubble,
        ].join(' ')}
      >
        {m.body}
      </div>
    </div>
  )
}

function PresenceDot ({ connected }: { connected: boolean }) {
  return (
    <span
      title={connected ? 'Online' : 'No conversation'}
      className={[
        'h-2.5 w-2.5 rounded-full',
        connected
          ? 'bg-emerald-400 shadow-[0_0_6px_2px_rgba(74,222,128,0.45)]'
          : 'bg-neutral-700',
      ].join(' ')}
    />
  )
}
