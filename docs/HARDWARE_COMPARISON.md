# Hardware Comparison: Pebble Time (2015) vs Pebble Time 2 (2026)

Side-by-side specs so we know exactly what each watch can and can't do.

---

## SoC & CPU

| | Pebble Time (2015) | Pebble Time 2 (2026) |
|---|---|---|
| **Platform codename** | basalt | emery |
| **SoC** | ST STM32F439ZG | SiFli SF32LB52JUD6 |
| **CPU core** | ARM Cortex-M4 (single) | ARM Cortex-M33 x2 (big.LITTLE) |
| **Clock speed** | 180 MHz (throttled to 100 MHz for power) | 240 MHz (HP core) + 24 MHz (LP core) |
| **FPU** | Yes (not exposed to apps) | Yes |
| **Architecture** | ARMv7E-M | ARMv8-M (TrustZone capable) |
| **GPU** | None | ePicaso 2.0 — 2D/2.5D, HW rotation/scaling up to 512x512 |
| **AI accelerator** | None | Neural network accelerator (NPU) |

**Implication:** The PT2 is roughly 5-10x the compute. big.LITTLE means the LP core handles BLE while the HP core runs your app — no CPU sharing. GPU means smooth animations and complex rendering that would stutter on the Time.

---

## Memory

| | Pebble Time (2015) | Pebble Time 2 (2026) |
|---|---|---|
| **SRAM** | 256 KB | 512 KB (HP core) + 64 KB (LP core) |
| **PSRAM** | None | 16 MB (OPI-PSRAM) |
| **App heap** | ~64 KB (basalt) | Significantly more (TBD) |
| **Flash (external)** | 16 MB (Spansion S29VS128R) | 32 MB (GD25Q256E, 256 Mbit QSPI NOR) |

**Implication:** PT2 has ~2x SRAM plus 16 MB PSRAM overflow. The old Time's 256 KB SRAM was the hard ceiling for everything (OS + app + framebuffer). PT2's apps can be much larger and hold more data in memory. The PSRAM is a game-changer for anything data-heavy (PPG buffers, large images, caching).

---

## Display

| | Pebble Time (2015) | Pebble Time 2 (2026) |
|---|---|---|
| **Type** | Color e-paper (MIP LCD) | Color e-paper (MIP LCD) |
| **Panel** | Sharp/JDI | JDI LPM015M135A |
| **Size** | 1.25" | 1.5" |
| **Resolution** | 144 x 168 | 200 x 228 |
| **PPI** | ~176 | ~202 |
| **Colors** | 64 | 64 |
| **Touch** | No | Yes (CST816D capacitive) |
| **Backlight** | White LED | RGB LED (AW2016 driver) |
| **Glass** | Hardened | Gorilla Glass, optically bonded |

**Implication:** PT2 has ~1.9x the pixels and a touchscreen. The RGB backlight can show colored illumination (not just white). Both are equally sunlight-readable (e-paper). UI designs must account for the different resolutions — same 64-color palette though.

---

## Bluetooth

| | Pebble Time (2015) | Pebble Time 2 (2026) |
|---|---|---|
| **BLE chip** | TI CC2564B (external) | Integrated in SF32LB52J |
| **BT version** | Bluetooth 4.0 (BLE 4.0) | Bluetooth 5.3 (BLE 5.3) |
| **Classic BT** | Yes (dual-mode via CC2564B) | Yes (dual-mode) |
| **BLE stack** | Proprietary (on CC2564B) | NimBLE (open source, on LP core) |
| **Max connections** | Limited (typically 1) | Multiple (NimBLE supports multi-conn) |
| **BLE features** | Basic BLE 4.0 | LE 2M PHY, coded PHY, extended advertising, improved security |
| **Power (BLE connected)** | Higher (external chip, older process) | ~50 µA (integrated, 24 MHz LP core) |

**Implication for BLE Key project:** This is the critical difference.
- **PT2:** BLE 5.3, NimBLE, multi-connection support, integrated. Can likely maintain phone + scooter connections simultaneously. Custom GATT services via open-source stack. **Feasible.**
- **Pebble Time:** BLE 4.0, external TI chip with proprietary stack, likely single connection. Adding a custom GATT service to a closed BLE stack is extremely difficult. **Probably not feasible** for the BLE key without major reverse engineering of the CC2564B firmware.

---

## Sensors

| | Pebble Time (2015) | Pebble Time 2 (2026) |
|---|---|---|
| **IMU** | Bosch BMI160 (6-axis: accel + gyro) | ST LSM6DSOW (6-axis: accel + gyro) |
| **Secondary accel** | None | ST LIS2DW12 (low-power) |
| **Magnetometer** | Freescale MAG3110 | Memsic MMC5603NJ |
| **Heart rate** | None | Goodix GH3026 (PPG AFE, 6-ch, multi-wavelength, 24-bit) |
| **Ambient light** | Yes (type unknown) | W1160 |
| **Temperature** | None | On-chip (SF32LB52) |
| **Barometer** | None | Reported in some specs (TBC) |

**Implication:** The PT2 has the HR sensor — the entire BP and HRV projects are PT2-only. Both watches have compass (magnetometer) and motion sensing. The Time's BMI160 is still a solid IMU.

---

## Audio

| | Pebble Time (2015) | Pebble Time 2 (2026) |
|---|---|---|
| **Microphone** | Yes (single) | Yes (dual PDM) |
| **Speaker** | None | Yes (waterproof, AW8155BFCR amp) |
| **Audio codec** | None | 24-bit ADC + DAC (8k-48k Hz) |

**Implication:** PT2 can play audio (tones, voice, alerts). The Time can only listen (voice dictation was supported).

---

## Haptics

| | Pebble Time (2015) | Pebble Time 2 (2026) |
|---|---|---|
| **Motor type** | ERM (eccentric rotating mass) | LRA (linear resonant actuator, AW86225CSR) |
| **Feel** | Buzzy, imprecise | Sharp, precise, quiet |
| **Programmable** | Basic on/off/pattern | Fine-grained waveform control |

**Implication:** PT2's LRA enables nuanced haptic patterns (distinct lock vs unlock vs notification feel). The Time's ERM is more of a blunt buzz.

---

## Power

| | Pebble Time (2015) | Pebble Time 2 (2026) |
|---|---|---|
| **Battery** | 150 mAh | 160 mAh |
| **PMIC** | Simple LDO (LT3009) | Nordic nPM1300 (Iq ~12 µA) |
| **Charge time** | ~2 hours | ~45 minutes |
| **Battery life** | ~7 days | ~10-30 days (varies, features still being enabled) |
| **FPGA** | Lattice iCE40 LP1K (display driver) | None (GPU handles display) |

**Implication:** Similar battery capacity but the PT2 is far more power-efficient (modern SoC, integrated BLE, no external FPGA). The nPM1300 PMIC is a proper smartwatch power manager vs the Time's basic regulator.

---

## Connectivity & Other

| | Pebble Time (2015) | Pebble Time 2 (2026) |
|---|---|---|
| **NFC** | No | No |
| **GPS** | No (phone relay) | No (phone relay) |
| **USB** | Magnetic charging only | Magnetic charging only |
| **Water resistance** | 30m (3 ATM) | Waterproof (rating TBC) |
| **Case** | Plastic with steel bezel | CNC 316 stainless steel, ceramic PVD |
| **Strap** | 22mm quick-release | 22mm quick-release |
| **OS** | PebbleOS (proprietary, now open) | PebbleOS (open source from day 1) |
| **Buttons** | 4 (back, up, select, down) | 4 (same layout) |

---

## Software / Developer Differences

| | Pebble Time (2015) | Pebble Time 2 (2026) |
|---|---|---|
| **SDK language** | C only | C + Alloy (JavaScript) |
| **Alloy support** | No | Yes |
| **App heap** | ~64 KB | Larger (TBD exact) |
| **BLE stack** | Closed (TI CC2564B firmware) | Open (NimBLE) |
| **Custom GATT** | Very difficult | Possible (firmware mod) |
| **Firmware source** | Partially open (PebbleOS) | Fully open (PebbleOS + BLE FW) |

---

## Project Feasibility Matrix

| Project | Pebble Time (2015) | Pebble Time 2 (2026) | Notes |
|---|---|---|---|
| **Bildschirmli Face** | Yes (C only, 144x168) | Yes (C or Alloy, 200x228) | Both work, different resolutions |
| **PT2-BP** | **No** | Yes | Needs GH3026 PPG sensor |
| **Heart Coherence** | **No** | Yes | Needs HR sensor |
| **Kumpan BLE Key** | **Unlikely** | Yes | Needs open BLE stack + multi-conn |

**Bottom line:** The Pebble Time (2015) can run the Bildschirmli watchface — that's it. Every other project needs PT2 hardware (PPG sensor or open BLE stack).

---

## Sources

- [iFixit Pebble Time Teardown](https://www.ifixit.com/Teardown/Pebble+Time+Teardown/42382)
- [Zephyr PT2 Board Docs](https://docs.zephyrproject.org/latest/boards/coredevices/pt2/doc/index.html)
- [SiFli SF32LB52J — CNX Software](https://www.cnx-software.com/2025/05/14/sifli-sf32lb52j-big-little-arm-cortex-m33-bluetooth-mcu-powers-the-core-time-2-smartwatch/)
- [Eric Migicovsky — How To Build A Smartwatch: Picking A Chip](https://ericmigi.com/blog/how-to-build-a-smartwatch-picking-a-chip/)
- [Pebble Developer Hardware Info](https://developer.repebble.com/guides/tools-and-resources/hardware-information/)
- [STM32F439ZG Datasheet](https://www.st.com/resource/en/datasheet/stm32f439zi.pdf)
- [SiFli Wiki — SF32LB52 Overview](https://wiki.sifli.com/en/hardware/SF32LB52/SF32LB52-Overview-B3.html)
