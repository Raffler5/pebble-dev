# Pebble Dev Guide

Knowledge base for developing watchfaces and apps for the rePebble ecosystem, with a focus on **Pebble Time 2 (PT2 / Emery)**.

---

## Project Ideas

| Project | Folder | Description | Complexity |
|---------|--------|-------------|------------|
| [Bildschirmli Watchface](bildschirmli-face/) | `bildschirmli-face/` | Retro clock + public transport departures | Low-Medium |
| [PT2-BP](pt2-bp/) | `pt2-bp/` | Cuffless blood pressure classification from raw PPG | High (research) |
| [Heart Coherence](heart-coherence/) | `heart-coherence/` | Real-time HRV coherence + breathing guide | Medium |
| [Kumpan BLE Key](kumpan-ble-key/) | `kumpan-ble-key/` | Pebble as wireless scooter key (port of ESP32 BLE key) | Medium-High |

See also: **[Hardware Comparison: Pebble Time (2015) vs PT2 (2026)](HARDWARE_COMPARISON.md)** — full side-by-side specs and project feasibility matrix.

---

## Table of Contents

- [Hardware Platforms](#hardware-platforms)
- [PT2 (Emery) Specs](#pt2-emery-specs)
- [Development Environment](#development-environment)
- [Language Choice: C vs Alloy (JS)](#language-choice-c-vs-alloy-js)
- [Alloy (JavaScript)](#alloy-javascript)
- [C SDK](#c-sdk)
- [Watch ↔ Phone Communication](#watch--phone-communication)
- [Memory & Performance](#memory--performance)
- [Do's and Don'ts](#dos-and-donts)
- [Publishing to the Appstore](#publishing-to-the-appstore)
- [Resources](#resources)

---

## Hardware Platforms

| Platform   | Codename | Display        | Resolution | Colors | Shape |
|------------|----------|----------------|------------|--------|-------|
| Pebble OG / Steel | aplite | E-paper | 144x168 | B&W (2) | Rect |
| Time / Time Steel | basalt | Color e-paper | 144x168 | 64 | Rect |
| Time Round | chalk | Color e-paper | 180x180 | 64 | Round |
| Pebble 2 / 2 SE | diorite | E-paper | 144x168 | B&W (2) | Rect |
| **Time 2** | **emery** | Color e-paper | **200x228** | **64** | **Rect** |
| Round 2 | gabbro | Color e-paper | 260x260 | 64 | Round |

**Alloy (JS) only supports Emery and Gabbro.** C apps can target all platforms.

---

## PT2 (Emery) Specs

- **Display:** 1.5" 64-color e-paper, 200x228 px, optically bonded Gorilla Glass
- **CPU:** ARM Cortex-M7
- **Case:** CNC milled 316 stainless steel with ceramic PVD coating
- **Battery:** ~10-30 days (depends on usage, some features still being enabled)
- **Sensors:** Heart rate monitor, accelerometer, compass, ambient light
- **Other:** Touchscreen, waterproof speaker, linear resonant actuator (vibration), microphone (x2, second not yet enabled), quick-release 22mm strap
- **Connectivity:** Bluetooth, NFC
- **OS:** PebbleOS (100% open source)

---

## Development Environment

### Option 1: CloudPebble (Fastest Start)

Browser-based IDE at [cloudpebble.repebble.com](https://cloudpebble.repebble.com/) — no install needed.

- Code editor with completion & syntax checking
- Built-in compiler
- Emulator for testing
- GitHub integration
- Auto-generates screenshots/GIFs for all platforms on publish

### Option 2: Local SDK

```bash
# See https://developer.repebble.com/sdk/ for current instructions
# Requires Python 3.13 (3.14 not yet supported by pebble-tool as of SDK 5.0.18)
# Does NOT run natively on Windows — use WSL with Ubuntu
```

Install methods:
- **Debian/Ubuntu:** Follow the guide at [developer.repebble.com/sdk](https://developer.repebble.com/sdk/)
- **Arch Linux:** Separate guide available
- **macOS:** Supported, see SDK docs
- **Windows:** Use WSL (Ubuntu), then follow Linux instructions
- **Docker:** `pebble/cloudpebble-composed` repo for local CloudPebble

### Option 3: Cloud VS Code

Browser-based VS Code with Pebble SDK pre-installed — see [developer.repebble.com/sdk](https://developer.repebble.com/sdk/).

---

## Language Choice: C vs Alloy (JS)

| Aspect | C (SDK 4) | Alloy (JavaScript) |
|--------|-----------|---------------------|
| Platform support | All (aplite → gabbro) | Emery + Gabbro only |
| Maturity | Battle-tested, huge example base | New (out of developer preview Feb 2026) |
| UI frameworks | TextLayer, BitmapLayer, custom draw | Piu (declarative) + Poco (low-level) |
| Language | C99 | ES2025 (ES6++) |
| Phone communication | AppMessage (key-value dict) | Standard Web APIs |
| Best for | Max compatibility, legacy ports | Modern PT2/PR2 apps, rapid prototyping |

**Recommendation for PT2-only development:** Start with Alloy — modern JS, better DX, Piu/Poco are powerful. Fall back to C if you need older platform support or hit Alloy limitations.

---

## Alloy (JavaScript)

Alloy is built on the [Moddable SDK](https://www.moddable.com/) and provides:

- Standard Web APIs
- ECMA-419 Embedded JavaScript APIs
- PebbleOS-specific APIs

### UI Frameworks

#### Piu (Declarative UI)
- Component-based, cascading styles
- Automatic layout
- Best for: standard app UIs, settings screens, list-based interfaces

#### Poco (Low-Level Graphics)
- Direct pixel control
- Manual rendering loop
- Best for: custom watchfaces, games, data visualizations

#### Port (Hybrid)
- Custom Poco drawing inside a Piu layout
- Best of both worlds

### Getting Started with Alloy

1. Open [CloudPebble](https://cloudpebble.repebble.com/)
2. Create new project, select Alloy
3. Follow the [official watchface tutorial](https://developer.repebble.com/tutorials/alloy-watchface-tutorial/part1/)
4. Check the [Moddable Pebble Examples](https://github.com/moddable-OpenSource/pebble-examples) repo

---

## C SDK

### Core Concepts

```
App
 └─ Window (one or more)
     └─ Layers (stacked, composited)
         ├─ TextLayer — text rendering with GFont
         ├─ BitmapLayer — image display with GBitmap
         └─ Layer — custom drawing via update_proc callback
```

### Lifecycle

```c
// 1. init() — called on app start
//    → create Window, push it
// 2. main_window_load() — window is visible
//    → create layers, add to window
// 3. main_window_unload() — window disappearing
//    → destroy layers
// 4. deinit() — app shutting down
//    → destroy window
```

**Every `_create()` must have a matching `_destroy()`.** Memory leaks are the #1 bug.

### Key APIs

- **TickTimerService** — subscribe for time updates (MINUTE_UNIT, SECOND_UNIT, etc.)
- **BatteryStateService** — battery level callbacks
- **ConnectionService** — Bluetooth status
- **AccelerometerService** — motion data
- **AppMessage** — phone ↔ watch communication (key-value dictionaries)
- **Persistent Storage** — persist_write_int/string/data, up to 4KB per key

### Watchface Tutorial

Follow the [official C watchface tutorial](https://developer.repebble.com/tutorials/watchface-tutorial/part1/) which covers:
1. Basic time display
2. Custom fonts and colors
3. Weather via web content
4. Battery and Bluetooth status
5. Timeline Peek
6. Settings page (phone-side config)

---

## Watch ↔ Phone Communication

The watch has **no direct internet access**. All network traffic, GPS, and heavy computation is offloaded to the phone via Bluetooth. This is the core architectural pattern for any connected Pebble app.

```
┌─────────────────┐         Bluetooth         ┌─────────────────────────────┐
│   Pebble Watch   │ ◄──── AppMessage ────► │      Phone                   │
│                   │    (key-value tuples)    │                             │
│  - Display        │                          │  PebbleKit JS (sandboxed)   │
│  - Buttons/Touch  │                          │   ├─ XMLHttpRequest / fetch │
│  - Sensors        │                          │   ├─ WebSocket              │
│  - Vibration      │                          │   ├─ localStorage           │
│  - 4KB persist    │                          │   └─ navigator.geolocation  │
│                   │                          │                             │
│  C app / Alloy    │                          │  ── or ──                   │
│                   │                          │                             │
│                   │                          │  PebbleKit Android (native) │
│                   │                          │   └─ full Android APIs      │
└─────────────────┘                          └─────────────────────────────┘
```

### Three Communication Paths

#### 1. PebbleKit JS (PKJS) — the standard path

A JavaScript sandbox running **inside the Pebble phone app**. No separate companion app needed. This is what most watchfaces and apps use.

**What it gives you on the phone side:**

| API | Use case |
|-----|----------|
| `XMLHttpRequest` | HTTP requests to web APIs (weather, transit, etc.) |
| `WebSocket` | Persistent connections (live data feeds) |
| `localStorage` | Phone-side storage (much larger than watch's 4KB) |
| `navigator.geolocation` | GPS coordinates from the phone |

**C SDK example — phone side (PKJS):**

```javascript
// src/pkjs/index.js — runs on the phone inside the Pebble app
Pebble.addEventListener('appmessage', function(e) {
  // Watch asked for weather
  var url = 'https://api.open-meteo.com/v1/forecast?latitude=52.52&longitude=13.41&current_weather=true';

  var xhr = new XMLHttpRequest();
  xhr.onload = function() {
    var json = JSON.parse(this.responseText);
    var temp = Math.round(json.current_weather.temperature);

    // Send result back to watch
    Pebble.sendAppMessage({ 'TEMPERATURE': temp }, function() {
      console.log('Sent temp to watch');
    });
  };
  xhr.open('GET', url);
  xhr.send();
});
```

**C SDK example — watch side:**

```c
// Watch receives the temperature
static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *temp_tuple = dict_find(iter, MESSAGE_KEY_TEMPERATURE);
  if (temp_tuple) {
    int temp = temp_tuple->value->int32;
    static char buf[8];
    snprintf(buf, sizeof(buf), "%d°C", temp);
    text_layer_set_text(s_temp_layer, buf);
  }
}

// In init():
app_message_register_inbox_received(inbox_received_handler);
app_message_open(64, 64);  // inbox, outbox buffer sizes
```

**Requires `location` capability** in `package.json` for GPS:

```json
{
  "pebble": {
    "capabilities": ["location"]
  }
}
```

#### 2. PebbleKit Android — standalone companion apps

A Java/Kotlin library for building your own Android app that talks to the watch. Use this when you need more than what the PKJS sandbox provides.

- [PebbleKitAndroid2](https://github.com/pebble-dev/PebbleKitAndroid2) — modern rewrite, work-in-progress
- Full Android API access (notifications, background services, databases, etc.)
- Can launch intents, access files, run persistent services

**When to use:** companion apps with their own UI, background data sync, integration with other Android apps, anything the PKJS sandbox can't do.

#### 3. PebbleKit iOS — DISCONTINUED

Apple systematically restricted the capabilities Pebble needed (background execution, inter-app comms, notification replies). **Do not target iOS companion apps.** PKJS still works for basic phone-side tasks on iOS through the Pebble app itself.

### Alloy (JS) — Simplified Networking

In Alloy apps, install `@moddable/pebbleproxy` for transparent phone-proxied networking:

```javascript
// Feels like normal web code — but routes through PKJS on the phone
import fetch from "@moddable/pebbleproxy";

const response = await fetch("https://api.open-meteo.com/v1/forecast?...");
const data = await response.json();
// Use data directly in your watchface
```

No manual AppMessage wiring needed — the proxy handles it.

### The AppMessage Protocol

All paths use **AppMessage** under the hood:

- **Push-oriented** — both watch and phone can initiate messages
- **Acknowledged** — every message gets an ACK/NACK
- **Key-value tuples** — integer keys, typed values (int, string, bytes)
- **Buffer-based** — you set inbox/outbox sizes at init (keep them small, memory is precious)
- **One message at a time** — wait for ACK before sending the next

### Message Size Limits

- **Inbox/outbox buffers** are set by the app (typically 64–256 bytes for simple apps)
- **Max AppMessage size:** platform-dependent, but keep messages compact
- **Batch data:** if you need to send a lot, split across multiple messages
- **Use integer keys and compact values** — don't send JSON strings over AppMessage, parse on the phone side and send only what the watch needs

### Communication Do's and Don'ts

**DO:**
- Cache last-known data on the watch — show stale weather rather than a blank screen when BT drops
- Keep AppMessage payloads small — send processed results, not raw API responses
- Handle `app_message_outbox_failed` — retransmit or show an error indicator
- Use `localStorage` on PKJS for phone-side caching (avoid re-fetching on every wrist raise)
- Set `capabilities: ["location"]` only if you actually need GPS (prompts the user)

**DON'T:**
- Don't send raw JSON to the watch — parse on phone, send only the values needed
- Don't fire off HTTP requests on every tick — throttle to reasonable intervals
- Don't assume messages arrive instantly — BT has latency, show loading states
- Don't forget to open AppMessage with `app_message_open()` before sending/receiving
- Don't set huge inbox buffers "just in case" — that's heap you can't use elsewhere
- Don't target PebbleKit iOS — it's dead

---

## Memory & Performance

### Hard Limits

- **App heap:** ~24 KB on Aplite, more on Emery — but still very constrained
- **Apps load entirely into RAM** (code + static vars) on start
- **Resources** (images, fonts) only load when you call `resource_load()`

### Rules

1. **Always check `_create()` return values** — they return `NULL` when memory is exhausted
2. **Always check `malloc()` return values**
3. **Match every `_create()` with `_destroy()`** in the unload handler
4. **Watch app logs on exit** — PebbleOS prints heap usage stats, revealing leaks
5. **Load resources lazily** — only `resource_load()` what you need right now
6. **Prefer stack allocation** over heap where possible
7. **Keep images small** — use 1-bit PNGs for B&W platforms, palettized PNGs for color
8. **Don't hold large buffers** — process data in chunks

### Checking for Leaks

When your app exits, the system logs show:
```
[INFO] heap usage: used X, free Y, max used Z
```
If "used" doesn't return to near-zero on exit, you have a leak.

---

## Do's and Don'ts

### DO

- **Test on the emulator AND real hardware** — timing, display readability in sunlight, and vibration feel can only be judged on-device
- **Handle Bluetooth disconnection gracefully** — phone comms will fail, show a fallback
- **Use `PBL_IF_COLOR_ELSE()`** macro to handle color vs B&W platforms in C
- **Use platform-specific resources** — different image assets per platform (aplite/, basalt/, etc.)
- **Design for the bezel** — Emery's 200x228 display is tall, don't assume square
- **Respect Timeline Peek** — leave room at the bottom if your watchface supports it
- **Use `persist_*` API** for settings — survives app restarts and watch reboots
- **Profile memory usage** — check heap stats regularly during development
- **Use CloudPebble's screenshot generation** — it auto-generates marketing assets
- **Subscribe only to what you need** — MINUTE_UNIT not SECOND_UNIT if you don't show seconds (saves battery)

### DON'T

- **Don't forget to unsubscribe from services** in `deinit()` — causes crashes
- **Don't use hardcoded screen dimensions** — use `layer_get_bounds()` to be platform-adaptive
- **Don't assume always-connected** — Bluetooth drops; cache last-known data
- **Don't poll in a tight loop** — use event-driven services (TickTimerService, etc.)
- **Don't load all resources at startup** — lazy-load to conserve memory
- **Don't ignore the 24KB heap limit** — every byte counts, especially with images
- **Don't skip error handling on AppMessage** — phone comms are unreliable
- **Don't use `SECOND_UNIT` ticks on watchfaces unless necessary** — major battery drain
- **Don't hardcode colors** — use `GColorFromHEX()` and handle B&W fallback
- **Don't assume touch is available** — only Emery and Round 2 have touchscreens
- **Don't modify the `.pbw` after build** — it's a signed archive

### PT2-Specific Tips

- **200x228 resolution** — more pixels than older platforms, design assets accordingly
- **Touchscreen** — you can use tap/swipe gestures, but keep button nav as primary (glanceable use case)
- **Speaker** — can play tones/tunes, read text (API still maturing)
- **Heart rate** — `HealthService` API for HR data, useful for fitness faces
- **Larger heap** — more room than Aplite, but still not desktop-class; don't get sloppy

---

## Publishing to the Appstore

### From CLI (SDK 4.9.148+)

```bash
pebble build
pebble publish
```

The `pebble publish` command handles screenshot generation and upload.

### From CloudPebble

CloudPebble auto-generates GIFs and screenshots for all supported platforms.

### Manual Submission

1. Build your `.pbw` with `pebble build`
2. Go to [rebble.io/submit](https://rebble.io/submit)
3. Fill in app details
4. Upload `.pbw`
5. Email `support@rebble.io` for approval

### Requirements

- Unique and valid UUID
- At least one `.pbw` release
- Required marketing assets (screenshots, icon, description)
- Follow the [submission guidelines](https://developer.repebble.com/community/submission/)

---

## Resources

### Official

- [rePebble Developer Portal](https://developer.repebble.com/)
- [SDK Documentation](https://developer.repebble.com/docs/)
- [SDK Installation](https://developer.repebble.com/sdk/)
- [CloudPebble](https://cloudpebble.repebble.com/)
- [Alloy Guide](https://developer.repebble.com/guides/alloy/)
- [C Tutorials](https://developer.repebble.com/tutorials/)
- [Hardware Info](https://developer.repebble.com/guides/tools-and-resources/hardware-information/)
- [Submission Guidelines](https://developer.repebble.com/community/submission/)
- [Pebble Appstore](https://apps.repebble.com/)

### Community / Legacy

- [Rebble Developer Docs](https://developer.rebble.io/) (legacy SDK docs mirror)
- [Rebble Help](https://help.rebble.io/)
- [Rebble Discord](https://discord.gg/rebble) — `#sdk-dev` channel for help
- [Core Devices GitHub](https://github.com/coredevices) — PebbleOS source
- [Moddable Pebble Examples](https://github.com/moddable-OpenSource/pebble-examples) — Alloy example apps
- [pebble-examples (C)](https://github.com/pebble-examples) — Classic C examples
- [Learning C with Pebble](https://pebble.gitbooks.io/learning-c-with-pebble/) — Free book

### Debugging

- [Common Runtime Errors](https://developer.repebble.com/guides/debugging/common-runtime-errors/)
- [Debugging with App Logs](https://developer.rebble.io/guides/debugging/debugging-with-app-logs/)
- [FAQ](https://developer.repebble.com/faqs/)

### Blog Posts

- [Coconauts: Making Pebble Apps in 2026](https://coconauts.net/blog/2026/05/08/pebble-apps/)
- [Rich Infante: Building Pebble Watchfaces on Modern Systems](https://www.richinfante.com/2025/02/09/building-pebble-watchfaces-on-modern-systems-sdk)
- [Moddable: Introducing Alloy](https://www.moddable.com/blog/pebble/)
