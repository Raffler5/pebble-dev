# PT2-BP — Cuffless Blood Pressure Classification

Estimate a coarse blood-pressure *category* (low / ok / high) from the Pebble Time 2's wrist PPG sensor — a hobby/research project, **explicitly not a medical device**.

## Status

Scoping / feasibility confirmed. No code written yet.

---

## Disclaimer

This is a maker/research project that produces a **trend indicator**, not a diagnosis. It is not a medical device, is not regulated, is not validated, and must never be used to make health decisions, dose medication, or replace a real cuff. Cuffless BP from a single wrist optical sensor is inherently a *calibrated estimate* that drifts and fails for irregular pulses. Treat every output as "interesting signal," not "blood pressure."

---

## Objective & Scope

| | |
|---|---|
| **In scope** | 3-class resting BP estimate (low / ok / high) from wrist PPG; personal (single-user) model; resting + still measurement only |
| **Out of scope** | Exact mmHg regression; continuous beat-to-beat BP; BP during motion/exercise; multi-user generalization; any clinical claim |
| **Success criterion** | On held-out personal data, ≥80% correct 3-class assignment + sensible day/night trend tracking vs. a cuff |
| **Hard constraint** | Single optical site, no ECG, no second PPG location → morphology-based only, no pulse-transit-time |

Classification is the right target: binning is far more forgiving of noise and drift than regression. You don't need ±5 mmHg accuracy — you need to separate "elevated" from "normal" from "low" reliably enough to be useful as a trend nudge.

---

## Why This Is Feasible on the PT2

The single most important fact:

> The PT2's heart-rate chip is a **PPG analog front-end (AFE)** that streams raw optical samples to the host. The previous Pebble (2016) used a sealed sensor that only output processed HR.

| Era | Chip | Access |
|-----|------|--------|
| Old Pebble 2 (2016) | AMS AS7000 | Sealed SoC, on-chip DSP, outputs only HR/HRV over I²C. Raw PPG never leaves the chip. |
| **New PT2 (2025/26)** | **Goodix GH3026** | AFE only — drives LEDs, digitizes photodiodes, hands **raw 24-bit PPG** to host MCU. The Goodix algo blob runs on the host, not the sensor. Raw stream is accessible upstream of it. |

---

## Hardware Reference — PT2 (codename "obelix")

From `coredevices/hardware` → `watch/Pebble Time 2 (obelix)`:

| Function | Part | Notes |
|---|---|---|
| **PPG / HR AFE** | **Goodix GH3026** | I²C, HR_INT, HR_RST; drives HR_LEDs. The component this project hinges on. |
| Host SoC | SiFli SF32LB52JUD6 | 43 GPIOs |
| IMU | ST LSM6DSOW | 6-axis; motion-gating ("measure only when still") |
| Accelerometer | ST LIS2DW12 | secondary |
| Magnetometer | MMC5603NJ | compass |
| PMIC | Nordic nPM1300 | Iq ~12 µA |
| Display | LPM015M135A | 1.5" 64-color e-paper, 200×228, touch (CST816T) |

### GH3026 Capabilities

- 2 independent AFEs, **synchronous sampling**
- Up to 8 programmable time-slots, 16 effective channels
- **Multi-wavelength**: green + red + IR (supports HR, HRV, SpO2)
- 24-bit ADC, FIFO with data-ready / almost-full interrupts
- Used in published BP research (τ-ring dataset uses this exact chip)

---

## Firmware Access Path

PebbleOS is fully open source (`github.com/coredevices/PebbleOS`):

| Item | Location |
|---|---|
| Driver | `src/fw/drivers/hrm/gh3x2x.c` (~487 lines) |
| Header | `src/fw/drivers/hrm/gh3x2x.h` |
| Kconfig flag | `CONFIG_HRM_GH3X2X` |
| Raw-data callback | `gh3x2x_rawdata_notify(uint32_t *p_rawdata, uint32_t data_count)` — raw 24-bit PPG |
| Channel count | `HRM_PPG_CH_NUM = 6` |
| FIFO watermark | 80 samples |
| Default sample rate | 25 Hz — **must raise to ≥100 Hz** |
| BLE raw stream | `src/bluetooth-fw/nimble/gh3x2x_tuning_service.c` — GATT NOTIFY characteristic, already pipes raw data |

### Firmware Tap (Conceptual)

1. **Raise sample rate** to ≥100 Hz (one config call, GH3026 supports it)
2. **Tap `gh3x2x_rawdata_notify()`** — 6-channel raw samples already flow here
3. **Stream via BLE** — reuse the existing tuning service NOTIFY characteristic
4. **Gate on motion** via LSM6DSOW — only capture clean still segments

---

## Method

Single-site PPG morphology / Pulse Wave Analysis (PWA):
- Extract features from the pulse *shape*, map to BP with a classifier
- **Per-user calibration** against a real cuff is mandatory
- **Drifts** over hours/days → needs periodic recalibration
- Breaks for irregular pulses — detect and abstain

### Benchmark: Aktiia/Hilo

The closest commercial analogue uses the same approach:
1. Mandatory cuff calibration every 30 days
2. Rest-only, motion-gated measurement
3. Statistical aggregation (~25 measurements/day), not spot readings
4. Contraindicated for AFib, HR >120, poor perfusion — confirms it's pure waveform inference

**The gap is not silicon** — your GH3026 is comparable. The gap is calibration discipline, rest-gating, and the algorithm + dataset.

---

## Feature Extraction

Standard PWA / morphology features (all computable by pyPPG):

- **Fiducial points:** systolic peak, dicrotic notch, diastolic peak, pulse onset
- **Timing:** upstroke time, systolic/diastolic widths at 10/25/50/75% amplitude
- **Amplitude/shape:** augmentation index, reflection index, area ratios
- **Derivative (APG/SDPPG):** a–b–c–d–e points and ratios (esp. b/a, aging index)
- **Multi-wavelength (PT2 bonus):** same features per channel (green/red/IR) + inter-channel ratios
- **Context:** HR, HRV, demographics (constant for single user)

---

## Open Datasets

| Dataset | Size | Why use it | Caveat |
|---|---|---|---|
| **PPG-BP (Liang 2018)** | 219 subjects | Best starting point, clean, on figshare | Fingertip, not wrist |
| **PulseDB** | 5.2M segments, 5,361 subjects | Big-data training, calib vs. calib-free studies | ICU/surgical, arterial-line BP |
| **τ-ring** (arXiv 2505.04172) | ring dataset | **Same GH3026 chip**, has BP labels | Ring, not wrist |
| **UCI Cuff-Less BP** | ~mil. records | More volume | Clinical, older |

---

## Toolkits

- **pyPPG** (`pyppg.readthedocs.io`) — 74 standardized biomarkers, fiducial detection, validated on PPG-BP
- **NeuroKit2** — broad PPG pipeline, beginner-friendly
- **HeartPy** — simple PPG processing
- `AI4HealthUOL/ppg-ood-generalization` — DL on PulseDB + OOD analysis
- `v3551G/BP-prediction-survey` — curated paper list
- `pedr0sorio/cuffless-BP-estimation` — full pipeline (MATLAB)

---

## Implementation Plan

### Phase 0 — Firmware Enablement (parallel track)
- [ ] Build PebbleOS, flash custom image over BLE
- [ ] Raise GH3026 sample rate to ~100 Hz
- [ ] Tap `gh3x2x_rawdata_notify`; expose 6-ch raw PPG over BLE NOTIFY
- [ ] Add motion-gating via LSM6DSOW
- [ ] Write phone/PC logger (BLE stream → CSV with timestamps)

### Phase 1 — Desk Prototype (no watch needed)
- [ ] Download PPG-BP; load with pyPPG
- [ ] Extract 74 biomarkers per subject's clean pulse
- [ ] Bin SBP into low / ok / high
- [ ] Train classifier (random forest / gradient boosting)
- [ ] Evaluate (confusion matrix, per-class recall, feature importances)
- [ ] Optional: repeat on PulseDB subset

### Phase 2 — On-Device Personal Model
- [ ] Collect personal labeled dataset (PT2 raw PPG + simultaneous cuff readings)
- [ ] Run pyPPG feature pipeline on wrist PPG
- [ ] Train personal classifier (public model = warm start only)
- [ ] Add recalibration loop (periodic cuff-paired re-collection)
- [ ] Deploy as watchface/app showing low/ok/high trend

---

## Classification Design

**Proposed bins (resting SBP):**

| Class | SBP (mmHg) | Note |
|---|---|---|
| low | < 90 | hypotension range |
| ok | 90 – 139 | normal → elevated |
| high | ≥ 140 | hypertension range |

- Start with SBP only (stronger morphology signal than DBP)
- Consider 2-class first (ok vs. not-ok) — simpler, arguably most useful
- Always carry a confidence / abstain option for low signal quality

---

## Data Collection Protocol (Phase 2)

- **Reference:** validated upper-arm oscillometric cuff (other arm), simultaneous with PPG
- **Stillness:** IMU-gated, 30–60s quiet sitting before each reading
- **Spread the range:** across day, after coffee, post-exercise rest, morning/evening, post-meal, stressed/relaxed
- **Volume:** hundreds of paired points across weeks
- **Log everything:** all 6 channels raw + timestamp + cuff SBP/DBP + context tag
- **Recalibrate:** periodically re-pair with cuff; design for drift

---

## Risks & Known Failure Modes

- **Domain gap (biggest):** clinical fingertip/arterial PPG → wrist reflective PPG generalizes poorly. Public data validates the pipeline, not the model.
- **25 Hz default too low** — fix before collecting anything
- **Drift:** mitigated by classification + recalibration, not eliminated
- **Irregular pulses:** arrhythmia / high HR / cold hands wreck morphology — detect and abstain
- **Motion artifacts:** dominant noise source — strict still-gating
- **Single-user only:** model won't transfer
- **Label noise:** cuff readings have variability; average multiple per capture
- **Class imbalance:** mostly "ok" data — deliberately oversample high/low states

---

## Open Decisions

- [ ] 2-class (ok / not-ok) vs. 3-class (low / ok / high) for v1?
- [ ] BLE streaming vs. on-flash buffering for data collection?
- [ ] Classifier on-watch vs. on-phone vs. on-server?
- [ ] Recalibration cadence (weekly? monthly?)?
- [ ] Use all 6 channels or down-select to best 2–3?

---

## References

- PebbleOS source: `github.com/coredevices/PebbleOS`
- PT2 hardware: `github.com/coredevices/hardware` → `watch/Pebble Time 2 (obelix)`
- Goodix GH3026: `goodix.com/en/product/sensors/health_sensors/ppg_afe/gh3026`
- PulseDB: `github.com/pulselabteam/PulseDB`
- PPG-BP dataset: "PPG-BP Liang 2018 figshare"
- τ-ring dataset: arXiv 2505.04172
- pyPPG: `pyppg.readthedocs.io`
- OOD generalization: `github.com/AI4HealthUOL/ppg-ood-generalization`
- BP survey: `github.com/v3551G/BP-prediction-survey`
