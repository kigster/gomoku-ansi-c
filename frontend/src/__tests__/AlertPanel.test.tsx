import { render, screen, act } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { describe, it, expect, vi, beforeEach } from 'vitest'
import AlertPanel, { showError, showInfo } from '../components/AlertPanel'

describe('AlertPanel', () => {
  beforeEach(() => {
    vi.useFakeTimers({ shouldAdvanceTime: true })
  })

  it('renders nothing initially', () => {
    const { container } = render(<AlertPanel />)
    expect(container.querySelectorAll('[role]')).toHaveLength(0)
  })

  it('shows an error alert when showError is called', () => {
    render(<AlertPanel />)
    act(() => showError('Something broke'))
    expect(screen.getByText('Something broke')).toBeInTheDocument()
  })

  it('shows an info alert when showInfo is called', () => {
    render(<AlertPanel />)
    act(() => showInfo('Game saved'))
    expect(screen.getByText('Game saved')).toBeInTheDocument()
  })

  it('shows multiple alerts simultaneously', () => {
    render(<AlertPanel />)
    act(() => {
      showError('Error 1')
      showInfo('Info 1')
      showError('Error 2')
    })
    expect(screen.getByText('Error 1')).toBeInTheDocument()
    expect(screen.getByText('Info 1')).toBeInTheDocument()
    expect(screen.getByText('Error 2')).toBeInTheDocument()
  })

  it('auto-dismisses info alerts after 5 seconds', () => {
    render(<AlertPanel />)
    act(() => showInfo('Temporary'))
    expect(screen.getByText('Temporary')).toBeInTheDocument()

    act(() => { vi.advanceTimersByTime(5100) })
    expect(screen.queryByText('Temporary')).not.toBeInTheDocument()
  })

  it('auto-dismisses error alerts after 8 seconds', () => {
    render(<AlertPanel />)
    act(() => showError('Persistent error'))
    expect(screen.getByText('Persistent error')).toBeInTheDocument()

    act(() => { vi.advanceTimersByTime(5100) })
    expect(screen.getByText('Persistent error')).toBeInTheDocument()

    act(() => { vi.advanceTimersByTime(3100) })
    expect(screen.queryByText('Persistent error')).not.toBeInTheDocument()
  })

  it('dismisses alert when X button is clicked', async () => {
    vi.useRealTimers()
    const user = userEvent.setup()
    render(<AlertPanel />)
    act(() => showInfo('Dismissable'))
    expect(screen.getByText('Dismissable')).toBeInTheDocument()

    const dismissBtn = screen.getByLabelText('Dismiss')
    await user.click(dismissBtn)
    expect(screen.queryByText('Dismissable')).not.toBeInTheDocument()
  })
})
