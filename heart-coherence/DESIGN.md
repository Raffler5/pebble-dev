# Heart Coherence / Stress Trainer

Real-time HRV coherence score + breathing guide on the Pebble Time 2.

## Concept

A watchface/app that uses the PT2's raw PPG sensor to compute heart rate variability in real-time, display a coherence score, and guide the user through resonance breathing to reduce stress.

This is a **great stepping stone** for the PT2-BP project — it validates the raw PPG firmware tap with a simpler, well-understood signal (HRV) before tackling BP classification.

## Why PT2

- **Goodix GH3026 raw PPG** — same sensor tap as PT2-BP, but HRV only needs inter-beat intervals (IBI), not full morphology. Works even at the default 25 Hz.
- **Always-on e-paper** — breathing guide visible without holding wrist up
- **Haptic feedback** — linear resonant actuator can pulse in/out rhythm for eyes-free breathing
- **Speaker** — can play gentle tones for breathing cues (API maturing)

## Features

### Core (v1)
- [ ] Real-time heart rate from raw PPG (or HealthService API if raw tap isn't ready)
- [ ] HRV computation (RMSSD, SDNN over rolling window)
- [ ] Coherence score (low / medium / high)
- [ ] Breathing guide: visual pacer (expanding/contracting circle or bar)
- [ ] Session timer (3 / 5 / 10 min selectable)
- [ ] Session summary (avg coherence, avg HR, HRV trend)

### v2
- [ ] Haptic breathing cues (vibration pulses for inhale/exhale timing)
- [ ] Resonance frequency finder (sweep breathing rates 4.5–7 breaths/min, find personal peak)
- [ ] Historical trend (daily coherence scores stored via persist API)
- [ ] Phone-side session log (export via PKJS)

## The Science (briefly)

**Heart coherence** = the degree to which heart rate oscillations are synchronized with breathing (respiratory sinus arrhythmia). High coherence correlates with parasympathetic activation and reduced stress.

**Resonance breathing** (~5.5–6 breaths/min for most people) maximizes HRV amplitude. The app guides the user to this rate and shows the coherence response in real-time.

**Metrics:**
- **RMSSD** — root mean square of successive IBI differences (parasympathetic marker)
- **SDNN** — standard deviation of IBIs (overall HRV)
- **Coherence score** — ratio of power in the resonance band (~0.04–0.15 Hz) to total HRV power (simple FFT or autocorrelation on IBI series)

## Architecture

```
PT2 Watch
┌─────────────────────────────────────┐
│  PPG Sensor (GH3026)                │
│    ↓ raw samples                    │
│  IBI Detection (peak-to-peak)       │
│    ↓ inter-beat intervals           │
│  HRV Engine (RMSSD, coherence)      │
│    ↓ scores                         │
│  UI (breathing pacer + metrics)     │
│  Haptics (vibration cues)           │
└─────────────────────────────────────┘
        │ session summary
        ▼
   Phone (PKJS) — log storage, export
```

All computation stays on-watch. Phone is only for logging/export. No network dependency = works anywhere.

## Firmware Dependency

**Minimal path:** use the existing `HealthService` API for HR — no firmware mod needed, but limited to ~1 Hz BPM updates (enough for basic HRV if IBI is exposed).

**Full path:** tap raw PPG via the same firmware mod as PT2-BP (Phase 0). This gives precise IBI timing and much better coherence resolution.

## Language Choice

**Alloy (Poco)** — real-time animated breathing pacer maps perfectly to Poco's direct pixel rendering. The coherence visualization (color shifts, expanding shapes) benefits from low-level drawing control.

## Open Questions

- [ ] Can HealthService expose IBI (not just BPM) without firmware mod?
- [ ] Breathing pacer style: circle expand/contract, bar rise/fall, or wave?
- [ ] Session data format for phone-side export?
- [ ] Integrate with Apple Health / Google Fit via phone companion?
