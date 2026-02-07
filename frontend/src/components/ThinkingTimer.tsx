import { useState, useEffect, useRef } from 'react'
import type { GamePhase } from '../types'

interface ThinkingTimerProps {
  phase: GamePhase
  playerName: string
}

export default function ThinkingTimer({ phase, playerName }: ThinkingTimerProps) {
  const [seconds, setSeconds] = useState(0)
  const intervalRef = useRef<ReturnType<typeof setInterval> | null>(null)
  const prevPhaseRef = useRef(phase)

  useEffect(() => {
    // Reset timer when phase changes to playing or thinking
    if (prevPhaseRef.current !== phase) {
      setSeconds(0)
    }
    prevPhaseRef.current = phase

    if (phase === 'playing' || phase === 'thinking') {
      intervalRef.current = setInterval(() => setSeconds(s => s + 1), 1000)
    }
    return () => {
      if (intervalRef.current) clearInterval(intervalRef.current)
    }
  }, [phase])

  if (phase !== 'playing' && phase !== 'thinking') return null

  const label = phase === 'thinking' ? 'AI thinking' : `${playerName} is thinking`

  return (
    <div className="mt-3 bg-neutral-800/80 rounded-lg px-4 py-3 text-center backdrop-blur-sm">
      <span className="text-white" style={{ fontSize: '14pt' }}>{label}:</span>
      <span className="text-amber-400 font-mono ml-2" style={{ fontSize: '14pt' }}>{seconds}s</span>
    </div>
  )
}
