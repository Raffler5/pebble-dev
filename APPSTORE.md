# Appstore Listing

## Name
Bildschirmli

## Tagline
Swiss transit departures in retro dot-matrix style

## Description
Bildschirmli brings Swiss public transport departure times to your wrist in a distinctive amber dot-matrix display style — like a real station departure board.

**How it works:**
- Launch the app and it finds nearby transit stations using your phone's GPS
- Pick a station from the list — sorted by distance, closest first
- See the next departures: line number, direction, and minutes until departure
- A bus icon shows when a connection is departing right now

**Features:**
- Amber phosphor dot-matrix aesthetic on black background
- GPS-based station discovery with configurable search radius (200m–2000m)
- Set a default home station that always appears in your list
- Auto-refresh every 60 seconds
- Shake your wrist to manually refresh
- Smart station names: abbreviates "Bahnhof" to "Bhf", strips redundant city prefixes
- Works on Pebble Time and Pebble Time 2

**Data source:** transport.opendata.ch — Swiss public transport API covering SBB, ZVV, BVB, and all Swiss transit operators.

## Category
Daily

## Source
https://github.com/Raffler5/bildschirmli-pebble

---

# Release Notes

## v0.2.0 — Settings & Polish

**New:**
- Settings page (tap gear icon in Pebble app)
  - Default station: set a home stop that always appears in the picker
  - Max distance: configure search radius (200m–2000m)
- Amber-themed settings page matching the app's dot-matrix look

**Improved:**
- Station names: shows stop part only when all stations share the same city (e.g. "BHF" instead of "LUFINGEN, BHF")
- Directions abbreviated: Bahnhof → Bhf, Strasse → Str
- Column layout rebalanced — more room for direction names
- Bus icon properly sized to match text row height

**Fixed:**
- Back button now works reliably (returns to station picker)
- No more phantom truncation dots on short text
- Station header no longer truncated to 2 characters

## v0.1.0 — Initial Release

Swiss transit departure board for Pebble Time and Pebble Time 2.

- GPS-based nearby station discovery
- Amber dot-matrix departure display
- Auto-refresh every 60 seconds
- Wrist shake to refresh
- Platform-adaptive layout (basalt 144×168, emery 200×228)
