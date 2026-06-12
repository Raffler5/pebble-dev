# Kumpan BLE Key — Pebble Edition

Use the Pebble Time 2 as a standalone wireless key for the Kumpan electric scooter, replicating the original nRF52 BLE key fob behavior.

**No phone needed. Watch talks directly to scooter.**

## Status

**Blocked.** PebbleOS does not expose BLE peripheral (GATT server) APIs to watchapps. The C SDK headers exist but are stubbed ("NOT SCOPED"), and Moddable's BLEServer was not ported to the Pebble platform. A custom PebbleOS firmware build would be required.

**Waiting for:** rePebble / Core Devices to implement the planned BLE server API. The headers and function signatures already exist in `src/fw/applib/bluetooth/` — just needs implementation. Revisit when PebbleOS gains app-level GATT server support (potentially late 2026).

As of 2026-06-12, no code written.

---

## Original Key Hardware & Firmware

The original Kumpan Key is an **nRF52832** (ARM Cortex-M4F) running Nordic nRF5 SDK with SoftDevice BLE stack. Firmware dump: `KEY.dat` (J-Flash format, 250 KB, created 2020-01-27). Full reverse engineering analysis in `/config/Desktop/Kumpan/kumpan-scooter/reference/oem_key_analysis/`.

We also built an **ESP32-C3 clone** (`/config/Desktop/Kumpan/kumpan-scooter/firmware/key_oem_clone/`) that replicates the behavior.

### Original Key Hardware

| Component | Detail |
|-----------|--------|
| **MCU** | Nordic nRF52832 (Cortex-M4F) |
| **BLE** | SoftDevice (Nordic proprietary stack), BLE 4.2 with LESC |
| **Accelerometer** | LIS2DH or LIS3DH (I²C) — wake-on-movement |
| **Buttons** | 2 physical buttons |
| **Battery** | Coin cell, measured via SAADC |
| **Firmware** | 1401 functions identified in Ghidra |

---

## BLE Protocol (from original firmware RE)

### GATT Profile

```
Device Name: "Kumpan Key"

Battery Service (0x180F) — Standard BLE
└─ Battery Level (0x2A19) — Read | Notify

Button Service — Custom
├─ Service UUID:        20082004-fb13-1000-1a80-fd6df7fd1a20
└─ Characteristic UUID: 20082005-fb13-1000-1a80-fd6df7fd1a20
                        ^^^^^^^^ note: byte 12 = 0x05 (not 0x04)
   ├─ Properties: Notify (+ optional Read)
   └─ Payload: 2 bytes [button_id, state]
      [0x01, 0x01] = Button 1 pressed  → UNLOCK
      [0x01, 0x00] = Button 1 released
      [0x02, 0x01] = Button 2 pressed  → LOCK
      [0x02, 0x00] = Button 2 released

Device Information Service (0x180A) — Standard
Generic Access (0x1800) — Standard
Generic Attribute (0x1801) — Standard
```

### Security

| Property | Value |
|----------|-------|
| **LESC** | Enabled (LE Secure Connections with ECDH) |
| **IO Capability** | NoIO (Just Works pairing) |
| **Bonding** | Required, stored in flash |
| **Whitelist** | Enabled after bonding |
| **MITM** | Configurable |

### Bonding Behavior (critical — has a lock-out!)

```
Power on with no peers  →  bonding ENABLED (open pairing mode)
Power on with peers     →  bonding DISABLED (whitelist only)

After successful bond:
  t=0s    "Starting BONDING DISABLE TIMER"
  t=60s   "Bonded since 60s! Bonding disabled forever!"
          → No new pairings until factory reset

Factory reset: press BOTH buttons simultaneously
  → pm_peers_delete() → reboot → open pairing mode
```

**Timer constants (from binary analysis):**

| Timer | Value | Purpose |
|-------|-------|---------|
| Button debounce | 50ms | GPIO debounce |
| Long press | 2000ms | Single-button long press |
| Bonding window | 120s | Time window for pairing after power-on |
| Bonded threshold | 60s | After 60s bonded → bonding disabled forever |
| ADV fast | 100ms (160 units) | Fast undirected advertising |
| ADV slow | 2000ms (3200 units) | Power-saving advertising |

### Advertising Modes

| Mode | Interval | Trigger |
|------|----------|---------|
| DIRECTED_HIGH_DUTY | 3.75ms | Button/movement, bonded peer known |
| DIRECTED | Variable | Fallback from high duty |
| FAST | 100ms | Undirected, no bonded peer or fallback |
| SLOW | 2000ms | Power saving after fast timeout |
| IDLE | — | No activity, low power |

Flow: button/movement → directed to bonded peer → fast undirected → slow → idle

---

## Unlock Chain

The key does **NOT** talk CAN. The scooter's **Speedometer** (dashboard) is the BLE central that bridges to CAN:

```
Pebble (BLE Peripheral)    Speedometer (BLE Central + CAN)    ECU-A / BMS
        │                              │                          │
        │◄──── BLE Connect ───────────│                          │
        │      (bonded, LESC encrypted)│                          │
        │                              │                          │
  User presses button on watch         │                          │
        │                              │                          │
        │── Notify [0x01, 0x01] ─────►│                          │
        │   (button1 pressed)          │── CAN unlock cmd ──────►│ Power ON
        │── Notify [0x01, 0x00] ─────►│                          │
        │   (button1 released)         │                          │
        │                              │                     Scooter ready
        │                              │                          │
  User presses button to lock          │                          │
        │                              │                          │
        │── Notify [0x02, 0x01] ─────►│                          │
        │   (button2 pressed)          │── CAN lock cmd ────────►│ Power OFF
        │── Notify [0x02, 0x00] ─────►│                          │
        │   (button2 released)         │                          │
```

### What the key MUST do

| Requirement | Value |
|-------------|-------|
| Button Service UUID | `20082004-fb13-1000-1a80-fd6df7fd1a20` |
| Button Char UUID | `20082005-fb13-1000-1a80-fd6df7fd1a20` |
| Char properties | Notify |
| Payload | 2 bytes: `[button_id, state]` |
| Security | LESC, bonded, encrypted |
| Bond persistence | Must survive power cycles |

### What the key does NOT need

- No CAN bus communication
- No UDS protocol knowledge
- No accelerometer (optional, wake-on-movement)
- No battery service (optional, cosmetic only)
- No specific advertising interval (any works, OEM uses 100ms/2000ms)

---

## Feasibility on PT2

### Does a regular watchapp have BLE peripheral access?

**No — but the API was planned and the plumbing exists.**

PebbleOS source code has BLE server API headers in `src/fw/applib/bluetooth/` with function signatures for custom GATT services, but they are **stubbed out** (marked "FUTURE / NOT SCOPED", never implemented):

| Function | Status |
|----------|--------|
| `ble_service_create()` | Declared, **not implemented** |
| `ble_server_start_service()` | Declared, **not implemented** |
| `ble_server_set_handlers()` | Declared, **not implemented** |
| `ble_server_send_update()` | Declared, **not implemented** |

Watchapps can only act as a BLE **client** (central). There is no way to register custom GATT services or send notifications from a regular app.

### So what's actually needed?

**A custom PebbleOS firmware build** — but it's a focused mod, not a rewrite:

1. **The API signatures already exist** — implement the ~5 stubbed functions in `src/fw/applib/bluetooth/`
2. **NimBLE underneath already supports it** — `ble_gatts_add_svcs()`, `ble_gattc_notify_custom()` work at the firmware level
3. **Precedent exists** — `gh3x2x_tuning_service.c` in PebbleOS already adds a custom GATT NOTIFY characteristic at firmware level. Same pattern.
4. **Multi-connection supported** — NimBLE handles phone + scooter connections simultaneously

Two implementation paths:

**Path A (cleaner, more work):** Implement the stubbed app-facing BLE server API so any watchapp can register GATT services. Then the key logic lives in a normal watchapp.

**Path B (quicker, dirtier):** Hard-code the Kumpan Button Service directly in the PebbleOS BLE firmware (like `gh3x2x_tuning_service.c`), wire it to a watchapp via an internal IPC mechanism.

### NimBLE supports everything we need

| Feature needed | NimBLE support |
|---|---|
| Custom 128-bit GATT service | Yes (`ble_gatts_add_svcs`) |
| GATT notifications | Yes (`ble_gattc_notify_custom`) |
| LESC (LE Secure Connections) | Yes (`ble_hs_cfg.sm_sc = 1`) |
| Just Works pairing | Yes (`ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO`) |
| Bond storage | Yes (NVS-backed) |
| Multi-connection | Yes (phone + scooter simultaneously) |

### Trade-offs of a custom firmware

| Pro | Con |
|-----|-----|
| Full control over BLE stack | No automatic OTA updates from rePebble |
| Can add any GATT service | Must manually merge upstream PebbleOS changes |
| Open source, forkable | Requires building PebbleOS toolchain |
| Could upstream the BLE server API | Risk of conflicts with PPoGATT (phone comms) |

---

## Implementation Plan

### Phase 1 — Study PebbleOS BLE Stack

- [ ] Map `src/bluetooth-fw/nimble/` — find GATT server init, service registration
- [ ] Study `gh3x2x_tuning_service.c` as the template for adding a custom service
- [ ] Determine if custom GATT services can be added from userspace or require firmware build
- [ ] Check max simultaneous connections (need ≥2: phone + scooter)

### Phase 2 — Add Kumpan GATT Service to PebbleOS

- [ ] Register Button Service (UUID `20082004-fb13-1000-...`)
- [ ] Register Button Characteristic (UUID `20082005-fb13-1000-...`, Notify)
- [ ] Implement 2-byte notification: `[button_id, state]`
- [ ] Configure LESC + Just Works + bonding
- [ ] Implement bonding persistence
- [ ] Test multi-connection: phone + scooter Speedometer

### Phase 3 — Watchapp UI

- [ ] Lock/unlock buttons (PT2 physical buttons or touchscreen)
- [ ] Send `[0x01, 0x01]` + `[0x01, 0x00]` for unlock (button1 press + release)
- [ ] Send `[0x02, 0x01]` + `[0x02, 0x00]` for lock (button2 press + release)
- [ ] Connection status indicator
- [ ] Confirmation haptics (LRA vibration on unlock/lock)
- [ ] Accidental press protection (hold or double-press)

### Phase 4 — Pairing with Speedometer

- [ ] Put Speedometer into pairing mode (may need to delete existing key bond on scooter side)
- [ ] Pebble advertises as "Kumpan Key" with Button Service UUID
- [ ] Speedometer connects, LESC pairs, bond stored
- [ ] After 60s bonded, bonding locks out (replicate or skip this timer?)
- [ ] **Open question:** does Speedometer filter by MAC or by service UUID?

### Phase 5 — Polish

- [ ] Watchface integration (lock icon on main face)
- [ ] Report real Pebble battery via Battery Service (0x180F)
- [ ] Optional: wake advertising on wrist movement (accelerometer)
- [ ] Security: optional PIN before unlock (anti-theft if watch stolen)
- [ ] Low-battery guard: stop advertising below 5%

---

## Open Questions

- [ ] Does PebbleOS allow adding GATT services from a watchapp, or firmware build only?
- [ ] Can PT2 maintain 2 BLE connections simultaneously (phone + Speedometer)?
- [ ] Does the Speedometer filter by BLE MAC address or only by GATT service UUID?
- [ ] Should we replicate the 60s bonding-forever-disable, or leave bonding open?
- [ ] The ESP32 clone uses char UUID `...1001...` but original firmware analysis says `...1005...` — which does the Speedometer actually expect? Need to verify with nRF Connect scan of original key.
- [ ] Power impact of maintaining second BLE connection + advertising?

---

## Reference Files

### Original Key Firmware (RE)

| File | Content |
|------|---------|
| `oem_key_analysis/KEY.dat` | Original nRF52 firmware dump (J-Flash, 250 KB) |
| `oem_key_analysis/KUMPAN_KEY_ANALYSIS.md` | Firmware overview, memory layout, function table |
| `oem_key_analysis/KUMPAN_KEY_BEHAVIOR.md` | Full behavioral spec, state machines, timer constants |
| `oem_key_analysis/OEM_KEY_RESET_GUIDE.md` | Factory reset procedure |
| `oem_key_analysis/ghidra_project/` | Ghidra disassembly project |

### ESP32-C3 Clone (our implementation)

| File | Content |
|------|---------|
| `key_oem_clone/main/gatt_svr.c` | GATT services (uses `...1001...` char UUID — may differ from original!) |
| `key_oem_clone/main/main.c` | BLE peripheral, advertising, bonding logic |
| `key_oem_clone/main/security.c` | HMAC-SHA256 activation |
| `key_oem_clone/pair_device.py` | UART pairing tool |

### PebbleOS BLE (to study)

| Path | What to look for |
|------|------------------|
| `src/bluetooth-fw/nimble/` | NimBLE integration, GATT server |
| `src/bluetooth-fw/nimble/gh3x2x_tuning_service.c` | Template: custom GATT NOTIFY service |

---

## Risks

- **Custom firmware required** — confirmed: PebbleOS BLE server API is stubbed, not implemented. Need to either implement it or hard-code the service at firmware level. Means forking PebbleOS and losing automatic OTA updates.
- **PPoGATT conflict** — PebbleOS uses PPoGATT for phone communication. Adding a second GATT service must not break phone connectivity. The `gh3x2x_tuning_service.c` precedent suggests coexistence works, but needs testing.
- **Multi-connection** — NimBLE supports it, but PebbleOS may have hard-coded a single-connection limit. Needs source code verification.
- **Speedometer compatibility** — connection parameters, timing, or MAC filtering may cause pairing to fail
- **Char UUID discrepancy** — original RE says `...1005...`, ESP32 clone uses `...1001...` — one of them is wrong. Must verify against real hardware with nRF Connect.
- **Security** — watch on wrist = scooter key. Stolen watch = stolen scooter without a PIN gate
