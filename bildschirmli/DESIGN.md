# Bildschirmli Watchface

Port of the [Bildschirmli](https://github.com/Raffler5/bildschirmli) retro clock + public transport display to the Pebble Time 2.

## Concept

The original Bildschirmli runs on an ESP8266 (SmallTV-Ultra) and shows:
- A retro-styled digital clock
- Next departures from a nearby public transport station

The PT2 is a natural home for this — always-on e-paper display, glanceable, sunlight-readable, and the phone handles all API calls.

## Why PT2

- **200x228 64-color e-paper** — perfect for the retro pixel aesthetic
- **Always-on display** — no wrist-raise needed, just glance
- **Phone offloads API calls** — transit APIs hit via PKJS, watch just renders
- **Battery for days** — no need to worry about polling drain if throttled sensibly

## Features

### Core (v1)
- [ ] Retro digital clock (port the Bildschirmli visual style)
- [ ] Next 2-3 departures from a configured station
- [ ] Line number + destination + minutes until departure
- [ ] Auto-refresh every 60s (phone-side fetch, push to watch)

### Nice to have
- [ ] Multiple stations (swipe or button to cycle)
- [ ] Delay/cancellation indicators (color-coded)
- [ ] Date + day of week
- [ ] Battery indicator
- [ ] BT disconnect fallback (show clock, hide stale transit data)
- [ ] Settings page (station selection, refresh interval)

## Architecture

```
PT2 Watch                          Phone (PKJS)
┌─────────────────┐                ┌──────────────────────┐
│  Watchface       │  AppMessage    │  Transit API client   │
│  - Clock render  │ ◄──────────► │  - Fetch departures   │
│  - Departure list│                │  - Parse + compact    │
│  - BT status     │                │  - localStorage cache │
│                  │                │  - Geolocation (opt.) │
└─────────────────┘                └──────────────────────┘
```

## Transit API

Depends on region. Options:
- **Swiss (SBB/ZVV):** `transport.opendata.ch/v1/stationboard`
- **German (DB/VBB/MVV):** HAFAS via `v6.db.transport.rest` or regional APIs
- **Generic:** any GTFS-RT feed

The phone-side JS fetches the API, parses the response, and sends only the compact departure tuples (line, destination, minutes) to the watch.

## Language Choice

**Alloy (JS)** — PT2-only target, modern DX, `@moddable/pebbleproxy` for transparent fetch. The retro clock rendering maps well to Poco (direct pixel drawing).

Alternatively **C SDK** if we want to support older Pebble models too — but the Bildschirmli aesthetic is best on Emery's larger 200x228 color display.

## Open Questions

- [ ] Alloy (Poco for retro rendering) vs. C SDK?
- [ ] Which transit API to target first? (Swiss? German?)
- [ ] Station selection: config page on phone, or GPS-based nearest station?
- [ ] How closely to replicate the original Bildschirmli pixel art vs. adapting for the watch form factor?
