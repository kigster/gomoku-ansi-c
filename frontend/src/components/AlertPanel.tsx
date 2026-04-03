import { useEffect, useState, useCallback } from 'react'

export type AlertType = 'error' | 'info'

export interface Alert {
  id: number
  type: AlertType
  message: string
}

let nextId = 0
let globalDispatch: ((alert: Omit<Alert, 'id'>) => void) | null = null

export function showAlert(type: AlertType, message: string) {
  globalDispatch?.({ type, message })
}

export function showError(message: string) {
  showAlert('error', message)
}

export function showInfo(message: string) {
  showAlert('info', message)
}

export default function AlertPanel() {
  const [alerts, setAlerts] = useState<Alert[]>([])

  const addAlert = useCallback((alert: Omit<Alert, 'id'>) => {
    const id = nextId++
    setAlerts(prev => [...prev, { ...alert, id }])
  }, [])

  const removeAlert = useCallback((id: number) => {
    setAlerts(prev => prev.filter(a => a.id !== id))
  }, [])

  useEffect(() => {
    globalDispatch = addAlert
    return () => { globalDispatch = null }
  }, [addAlert])

  return (
    <div className="fixed top-4 right-4 z-[100] flex flex-col gap-2 max-w-sm w-full pointer-events-none">
      {alerts.map(alert => (
        <AlertItem key={alert.id} alert={alert} onDismiss={() => removeAlert(alert.id)} />
      ))}
    </div>
  )
}

function AlertItem({ alert, onDismiss }: { alert: Alert; onDismiss: () => void }) {
  useEffect(() => {
    const ms = alert.type === 'error' ? 8000 : 5000
    const timer = setTimeout(onDismiss, ms)
    return () => clearTimeout(timer)
  }, [alert, onDismiss])

  const isError = alert.type === 'error'

  return (
    <div
      className={`pointer-events-auto rounded-lg px-4 py-3 shadow-lg border
        flex items-start gap-3 animate-slide-in
        ${isError
          ? 'bg-red-900/90 border-red-700 text-red-100'
          : 'bg-emerald-900/90 border-emerald-700 text-emerald-100'
        }`}
    >
      <span className="text-lg mt-0.5">{isError ? '\u26A0' : '\u2714'}</span>
      <p className="flex-1 text-sm">{alert.message}</p>
      <button
        onClick={onDismiss}
        className={`text-lg leading-none opacity-60 hover:opacity-100 transition-opacity
          ${isError ? 'text-red-300' : 'text-emerald-300'}`}
        aria-label="Dismiss"
      >
        &times;
      </button>
    </div>
  )
}
