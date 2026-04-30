import { render, screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { describe, it, expect, vi, beforeEach } from 'vitest'
import ChooseGameTypeModal from '../components/ChooseGameTypeModal'

vi.mock('../lib/multiplayerClient', async () => {
  const actual = await vi.importActual<
    typeof import('../lib/multiplayerClient')
  >('../lib/multiplayerClient')
  return {
    ...actual,
    newGame: vi.fn(),
    cancelGame: vi.fn(),
    getGame: vi.fn(),
  }
})

vi.mock('../hooks/useMultiplayerHostPolling', () => ({
  useMultiplayerHostPolling: () => ({
    view: null,
    secondsWaited: 0,
    expired: false,
    error: null,
  }),
}))

import {
  newGame,
  cancelGame,
  type MultiplayerGameView,
} from '../lib/multiplayerClient'

const mockNewGame = newGame as unknown as ReturnType<typeof vi.fn>
const mockCancelGame = cancelGame as unknown as ReturnType<typeof vi.fn>

const FAKE_GAME: MultiplayerGameView = {
  code: 'AB7K3X',
  state: 'waiting',
  board_size: 15,
  rule_set: 'freestyle',
  host: { username: 'host', color: 'X' },
  guest: null,
  moves: [],
  next_to_move: 'X',
  winner: null,
  your_color: 'X',
  your_turn: false,
  version: 0,
  color_chosen_by: 'host',
  expires_at: new Date(Date.now() + 15 * 60 * 1000).toISOString(),
  created_at: new Date().toISOString(),
  finished_at: null,
  invite_url: 'http://localhost/play/AB7K3X',
}

function renderModal(handlers: Partial<{
  onAIChosen: () => void
  onGuestJoined: (code: string) => void
  onClose: () => void
}> = {}) {
  return render(
    <ChooseGameTypeModal
      authToken='test-token'
      onAIChosen={handlers.onAIChosen ?? vi.fn()}
      onGuestJoined={handlers.onGuestJoined ?? vi.fn()}
      onClose={handlers.onClose ?? vi.fn()}
    />,
  )
}

describe('ChooseGameTypeModal', () => {
  beforeEach(() => {
    vi.clearAllMocks()
  })

  it('renders the title and AI/Another-Player radios with AI selected by default', () => {
    renderModal()
    expect(screen.getByText('Choose Game Type')).toBeInTheDocument()
    const ai = screen.getByLabelText(/AI/i, { selector: 'input' })
    const human = screen.getByLabelText(/Another Player/i, { selector: 'input' })
    expect(ai).toBeChecked()
    expect(human).not.toBeChecked()
    // Sub-questions are hidden until Another Player is picked.
    expect(screen.queryByText(/Who chooses the playing color/)).toBeNull()
  })

  it('reveals the color-chooser sub-radios when Another Player is picked', async () => {
    const user = userEvent.setup()
    renderModal()
    await user.click(screen.getByLabelText(/Another Player/i, { selector: 'input' }))
    expect(screen.getByText(/Who chooses the playing color/)).toBeInTheDocument()
    expect(
      screen.getByLabelText(/I will choose/i, { selector: 'input' }),
    ).toBeChecked()
    // X/O choices show because "I will choose" is the default.
    expect(screen.getByText(/Your color/)).toBeInTheDocument()
  })

  it('hides X/O choices when Opponent will choose is selected', async () => {
    const user = userEvent.setup()
    renderModal()
    await user.click(screen.getByLabelText(/Another Player/i, { selector: 'input' }))
    await user.click(screen.getByLabelText(/Opponent will choose/i, { selector: 'input' }))
    expect(screen.queryByText(/Your color/)).toBeNull()
  })

  it('Start with AI selection fires onAIChosen and does not POST', async () => {
    const user = userEvent.setup()
    const onAIChosen = vi.fn()
    renderModal({ onAIChosen })
    await user.click(screen.getByRole('button', { name: /^Start$/ }))
    expect(onAIChosen).toHaveBeenCalledTimes(1)
    expect(mockNewGame).not.toHaveBeenCalled()
  })

  it('Start with Another Player POSTs /multiplayer/new and shows the invite link', async () => {
    const user = userEvent.setup()
    mockNewGame.mockResolvedValueOnce(FAKE_GAME)
    renderModal()

    await user.click(screen.getByLabelText(/Another Player/i, { selector: 'input' }))
    await user.click(screen.getByRole('button', { name: /^Start$/ }))

    await waitFor(() => {
      expect(mockNewGame).toHaveBeenCalledWith('test-token', { host_color: 'X' })
    })
    await waitFor(() => {
      expect(screen.getByText(/Waiting for your opponent to join/)).toBeInTheDocument()
    })
    expect(screen.getByDisplayValue(FAKE_GAME.invite_url)).toBeInTheDocument()
  })

  it('Opponent-chooses sends host_color: null', async () => {
    const user = userEvent.setup()
    mockNewGame.mockResolvedValueOnce({
      ...FAKE_GAME,
      color_chosen_by: 'guest',
      host: { ...FAKE_GAME.host, color: null },
      your_color: null,
    })
    renderModal()
    await user.click(screen.getByLabelText(/Another Player/i, { selector: 'input' }))
    await user.click(
      screen.getByLabelText(/Opponent will choose/i, { selector: 'input' }),
    )
    await user.click(screen.getByRole('button', { name: /^Start$/ }))

    await waitFor(() => {
      expect(mockNewGame).toHaveBeenCalledWith('test-token', { host_color: null })
    })
  })

  it('closing in waiting state calls /cancel and onClose', async () => {
    const user = userEvent.setup()
    mockNewGame.mockResolvedValueOnce(FAKE_GAME)
    mockCancelGame.mockResolvedValueOnce({ ...FAKE_GAME, state: 'cancelled' })
    const onClose = vi.fn()
    renderModal({ onClose })

    await user.click(screen.getByLabelText(/Another Player/i, { selector: 'input' }))
    await user.click(screen.getByRole('button', { name: /^Start$/ }))
    await waitFor(() =>
      expect(screen.getByText(/Waiting for your opponent to join/)).toBeInTheDocument(),
    )

    // ModalShell's close button has aria-label "Close" (via ModalCloseButton).
    const closeButton = screen.getByLabelText(/close/i)
    await user.click(closeButton)

    await waitFor(() => {
      expect(mockCancelGame).toHaveBeenCalledWith('test-token', FAKE_GAME.code)
    })
    expect(onClose).toHaveBeenCalled()
  })

  it('shows an inline error when newGame fails', async () => {
    const user = userEvent.setup()
    mockNewGame.mockRejectedValueOnce(new Error('Network down'))
    renderModal()
    await user.click(screen.getByLabelText(/Another Player/i, { selector: 'input' }))
    await user.click(screen.getByRole('button', { name: /^Start$/ }))
    expect(await screen.findByText('Network down')).toBeInTheDocument()
  })
})
