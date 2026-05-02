import type { ReactNode } from 'react'

export type SidePanelTab = 'solo' | 'multi'

interface SidePanelTabsProps {
  active: SidePanelTab
  onChange: (next: SidePanelTab) => void
  /** Always-rendered Solo content; visibility is toggled with `hidden`
   *  rather than unmount so the SettingsPanel keeps its internal state
   *  (radius slider position, etc.) when the user flips tabs. */
  solo: ReactNode
  /** Always-rendered Multi content (the ChatPanel) — same retention
   *  rationale. */
  multi: ReactNode
}

/**
 * Tabs around the right-rail. "Solo" holds the AI settings + side picker;
 * "Multi" holds the chat panel. Both panels stay mounted so transient state
 * (chat draft, scroll position, slider value) survives a tab switch.
 *
 * Visual tone matches the rest of the right rail: warm-neutral panel with an
 * amber accent for the active tab. The active-tab indicator is a 2-pixel bar
 * underneath the label rather than the more common pill, so it reads as a
 * navigation primitive (like browser tabs) instead of an action button.
 */
export default function SidePanelTabs ({
  active,
  onChange,
  solo,
  multi,
}: SidePanelTabsProps) {
  return (
    <div className='flex flex-col'>
      <div
        role='tablist'
        aria-label='Right-rail panel'
        className='flex border-b border-neutral-700/80'
      >
        <TabButton
          label='Solo'
          isActive={active === 'solo'}
          onClick={() => onChange('solo')}
        />
        <TabButton
          label='Multi'
          isActive={active === 'multi'}
          onClick={() => onChange('multi')}
        />
      </div>
      <div className='pt-4'>
        <div role='tabpanel' hidden={active !== 'solo'}>
          {solo}
        </div>
        <div role='tabpanel' hidden={active !== 'multi'}>
          {multi}
        </div>
      </div>
    </div>
  )
}

function TabButton ({
  label,
  isActive,
  onClick,
}: {
  label: string
  isActive: boolean
  onClick: () => void
}) {
  return (
    <button
      type='button'
      role='tab'
      aria-selected={isActive}
      onClick={onClick}
      className={[
        'flex-1 py-2.5 text-sm font-semibold font-heading uppercase',
        'tracking-[0.18em] transition-colors',
        // The 2-px underline lives in `border-b-2` so the active state
        // doesn't shift content down on click — the border is always there,
        // just transparent on the inactive side.
        'border-b-2',
        isActive
          ? 'text-amber-300 border-amber-400'
          : 'text-neutral-500 hover:text-neutral-200 border-transparent',
      ].join(' ')}
    >
      {label}
    </button>
  )
}
