# PebbleOS Sleep Tracking Internals

How sleep detection actually works inside PebbleOS. Extracted from the
open-source PebbleOS documentation at `src/fw/services/activity/docs/index.md`
in the `coredevices/PebbleOS` repo.

The algorithm is called **Kraepelin** (named after the psychiatrist who
pioneered sleep research). Code lives in
`src/fw/services/activity/kraepelin/`.

---

## Core Inputs

Every minute, PebbleOS computes two values from the accelerometer:

**VMC (Vector Magnitude Counts):**
A measure of overall movement intensity. VMC = 0 when the watch is perfectly
still. Higher values = more motion. Calibrated to match Actigraph medical
devices (collaboration with Stanford Wearables lab).

**Orientation:**
An 8-bit quantized representation of the watch's 3D angle. Lower 4 bits =
angle in X-Y plane, upper 4 bits = angle relative to Z-axis. Computed from
the average accelerometer reading across all three axes per minute.

---

## Sleep Detection Algorithm

### Sleep Minutes

A **filtered VMC** is computed by convolving the raw VMC values with a window
of ±4 minutes around each minute. If the filtered VMC is below a sleep
threshold, that minute is a **sleep minute**.

### Sleep Session Rules

1. **Sleep starts** after at least **5 consecutive sleep minutes**
2. **Sleep continues** until at least **11 consecutive non-sleep minutes**
   - Exception: in the first 60 minutes, requires **14** non-sleep minutes to exit
3. **Sleep session is valid** only if it lasted at least **60 minutes**

### Immediate Wake

If any single minute has an exceptionally high filtered VMC during a sleep
session, the session ends immediately (no need for 11 consecutive non-sleep
minutes).

### 80% Rule

If at least 80% of minutes within a session have slight movement (even if
each individual minute isn't enough to be a non-sleep minute), the user is
considered awake and the session is invalidated.

---

## Restful (Deep) Sleep Detection

After a sleep session is identified, a second pass determines restful sleep
periods within it.

A **restful sleep minute** has a filtered VMC below a lower threshold (stricter
than normal sleep minutes).

1. **Restful sleep starts** after **20 consecutive restful sleep minutes**
2. **Restful sleep ends** after **1 minute** that is not restful

Maximum 8 deep sleep sessions tracked per sleep session
(`KALG_MAX_DEEP_SLEEP_SESSIONS`).

---

## Not-Worn Detection (Critical for Dedicated Sleep Watch)

This is the key logic for the "watch on desk all day, worn only at night"
use case.

### Not-Worn Minutes

A minute is a **not-worn minute** if EITHER:
- Raw VMC is below the "not worn" threshold **AND** orientation is unchanged
  from the prior minute
- The watch is charging

### Not-Worn Session Invalidation

A candidate sleep session is invalidated as "not worn" if BOTH:
1. There are at least **100 consecutive not-worn minutes** within the session
2. The not-worn section starts within **20 minutes of the session start** AND
   ends within **10 minutes of the session end**

### Why 100 Minutes?

The threshold seems long, but valid restful sleep sessions can approach 100
minutes of near-zero movement with consistent orientation. Setting it lower
would cause false not-worn detection during deep sleep.

### The Orientation Trick

A watch on a desk that gets occasional vibrations (floor shake, table bump)
produces small VMC spikes that look like sleep micro-movements. But the
orientation stays constant — the watch doesn't rotate on the desk.

During actual sleep, position changes cause **orientation shifts**. This is
the primary signal that distinguishes "sleeping with watch on" from "watch
on desk."

---

## Implications for Dedicated Sleep Watch Use Case

**Scenario:** Pebble watch sits on desk all day, put on wrist only at bedtime.

### What Happens During the Day (Watch on Desk)

- VMC ≈ 0 (maybe tiny vibrations from desk)
- Orientation = constant (watch doesn't rotate)
- Algorithm: classifies as "not-worn" ✓

### What Happens When Put on Wrist

- Sudden motion burst (picking up watch, strapping it on)
- Orientation changes (wrist angle ≠ desk angle)
- VMC increases (even subtle wrist micro-movements)
- Algorithm: starts seeing this as "potentially worn"

### What Happens During Sleep

- VMC low but non-zero (breathing, micro-movements)
- Orientation changes periodically (rolling over, shifting position)
- Algorithm: recognizes as sleep (not "not-worn") ✓

### The Risk: Immediate Sleep After Putting Watch On

If you put the watch on and fall asleep within minutes:
- The transition from "not-worn" (desk) to sleep is very abrupt
- The 100-minute not-worn run from the desk period might merge with
  the start of the sleep session
- If the not-worn section covers "within 20 minutes of session start"
  to "within 10 minutes of session end," the session gets invalidated

**Mitigation:** Put the watch on ~20 minutes before actually sleeping. Move
your wrist — read, use your phone, etc. This creates a clear "worn + awake"
period that separates desk-time from sleep-time.

### Summary

| Phase | VMC | Orientation | Classification |
|-------|-----|-------------|---------------|
| On desk all day | ≈ 0 | Constant | Not-worn ✓ |
| Picked up + strapped on | High spike | Changes | Worn, awake |
| Awake in bed (reading etc.) | Low-medium | Changing | Worn, awake |
| Sleeping | Very low | Changing periodically | Sleep ✓ |
| Deep sleep | Near zero | Mostly stable, occasional shift | Restful sleep ✓ |

**The algorithm should work correctly for this use case** as long as there's
a transition period (≥20 minutes) between putting the watch on and falling
asleep.

---

## API Access (Read-Only)

The HealthService API only **reads** sleep data. There is no way to:
- Start/stop sleep tracking programmatically
- Insert or modify sleep sessions
- Trigger the Kraepelin algorithm manually

Sleep tracking runs automatically in the background as part of Pebble Health.

### Useful API Functions

```c
// Check if user is currently sleeping
HealthActivityMask activities = health_service_peek_current_activities();
bool sleeping = (activities & HealthActivitySleep);
bool deep_sleep = (activities & HealthActivityRestfulSleep);

// Get sleep total for today
int sleep_seconds = health_service_sum_today(HealthMetricSleepSeconds);
int deep_seconds = health_service_sum_today(HealthMetricSleepRestfulSeconds);

// Iterate through sleep sessions
health_service_activities_iterate(
    HealthActivitySleep | HealthActivityRestfulSleep,
    time_start, time_end,
    HealthIterationDirectionPast,
    callback, context);
```

Requires `"health"` capability in `appinfo.json`.

---

## Source Code Locations

| File | Content |
|------|---------|
| `src/fw/services/activity/docs/index.md` | Full algorithm documentation (this file's source) |
| `src/fw/services/activity/kraepelin/kraepelin_algorithm.c` | Sleep + step algorithm implementation |
| `src/fw/services/activity/kraepelin/activity_algorithm_kraepelin.c` | Algorithm integration with activity service |
| `include/pbl/services/activity/kraepelin/kraepelin_algorithm.h` | Constants (min session length, deep sleep sessions) |
| `src/fw/services/activity/activity_sessions.c` | Sleep session storage and management |
| `src/fw/applib/health_service.c` | Public API (read-only access for apps) |

---

*Source: coredevices/PebbleOS (open source), activity service documentation*
*Extracted: 2026-06-16*
