# Swiss Transit API Patterns

Using `transport.opendata.ch` for Swiss public transport data.
Lessons from Bildschirmli (Pebble + SmallTV-Ultra).

---

## API Base

```
http://transport.opendata.ch/v1/
```

Plain HTTP — no TLS needed. On memory-constrained devices (ESP8266, Pebble via PKJS), this saves ~22KB heap vs HTTPS.

---

## Station Search

```
GET /v1/locations?query={text}&type=station
```

| Parameter | Required | Notes |
|-----------|----------|-------|
| `query` | Yes | Station name or ID. Min 2 chars for useful results. |
| `type` | Recommended | `station` filters out addresses and POIs |
| `limit` | Optional | Omit for all results, or set a number |

**Response:**
```json
{
  "stations": [
    {
      "id": "8503000",
      "name": "Zürich HB",
      "coordinate": { "x": 47.378, "y": 8.540 }
    }
  ]
}
```

**Gotcha — same-name stations:** Some stops have identical names but different IDs (one per direction, e.g. "Schmiede Wiedikon"). Show coordinates in the UI to disambiguate.

**Gotcha — coordinate fields:** `x` = latitude, `y` = longitude. Yes, this is confusing. The API uses Swiss convention where x is north-south.

---

## GPS-Based Nearest Stations

```
GET /v1/locations?x={latitude}&y={longitude}&type=station&limit=15
```

Returns stations sorted by distance from the given coordinates. Request more than you need (15) and filter client-side by your max distance setting.

**Distance calculation:** The API doesn't return distance. Calculate it yourself with Haversine:

```javascript
function haversineM(lat1, lon1, lat2, lon2) {
    var R = 6371000;
    var dLat = (lat2 - lat1) * Math.PI / 180;
    var dLon = (lon2 - lon1) * Math.PI / 180;
    var a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
            Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) *
            Math.sin(dLon / 2) * Math.sin(dLon / 2);
    return R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
}
```

---

## Stationboard (Departures)

```
GET /v1/stationboard?id={station_id}&limit=12
```

Or by name (less reliable):
```
GET /v1/stationboard?station={name}&limit=12
```

**Always use `id` when you have it.** Names can be ambiguous.

### Field Filtering (Critical for Constrained Devices)

The full response is ~50KB. With field filtering it's ~2KB:

```
GET /v1/stationboard?id=8503000&limit=12
    &fields[]=station/name
    &fields[]=stationboard/to
    &fields[]=stationboard/number
    &fields[]=stationboard/category
    &fields[]=stationboard/stop/departureTimestamp
    &fields[]=stationboard/stop/delay
    &fields[]=stationboard/stop/platform
    &fields[]=stationboard/stop/prognosis/platform
```

**On ESP8266 (80KB RAM):** Field filtering is mandatory — 50KB response would crash.
**On Pebble PKJS:** Field filtering is optional. Bildschirmli fetches without filtering because the detail view needs `passList` (route stops), which can't be efficiently filtered. The phone handles the 50KB fine.

### Key Fields

| Field | Type | Example | Notes |
|-------|------|---------|-------|
| `number` | string | `"2"`, `"36"` | Line number only (no category) |
| `category` | string | `"S"`, `"IR"`, `"IC"`, `"T"`, `"B"` | Transport type. Concatenate with number for display: "S2", "IR36" |
| `to` | string | `"Ziegelbrücke"` | Destination name |
| `stop.departureTimestamp` | int | `1718535600` | Scheduled departure (Unix timestamp) |
| `stop.delay` | int | `0`, `2`, `5` | Delay in minutes. Pre-computed by the API. |
| `stop.platform` | string | `"3"`, `"2A-D"`, `""` | Platform/track. Always set for trains/buses, empty for trams. |
| `stop.prognosis.platform` | string/null | `null` | Realtime platform (track change). Almost always null. |
| `passList` | array | (see below) | All intermediate stops with times |
| `operator` | string | `"SBB"` | Operator name. Messy formatting. Low value. |
| `capacity1st`/`capacity2nd` | null | — | Documented but never populated |

### GOTCHA: `prognosis.departureTimestamp` Does Not Exist

The API response does **not** contain `stop.prognosis.departureTimestamp`. It returns:
- `stop.prognosis.departure` — ISO 8601 string (e.g. `"2026-06-17T16:50:00+0200"`)
- `stop.delay` — integer minutes

If you request `fields[]=stationboard/stop/prognosis/departureTimestamp`, you get nothing back silently.

### Departure Time (Correct Approach)

Use scheduled timestamp + delay for the real ETA:

```javascript
var depTime = entry.stop.departureTimestamp || 0;
var delay = entry.stop.delay || 0;
var realDep = depTime + delay * 60;
var minutes = Math.max(0, Math.floor((realDep - now) / 60));
```

Do NOT try to use `prognosis.departureTimestamp` — it doesn't exist in the response.

### Platform Data

```
&fields[]=stationboard/stop/platform
&fields[]=stationboard/stop/prognosis/platform
```

- **Trains (S, IC, IR, RE):** Always populated (`"3"`, `"2A-D"`, `"41/42"`)
- **Buses (B):** Usually populated with bay letters (`"A"`, `"E"`)
- **Trams (T):** Empty string at tram-only stops, may have letter at mixed stops
- **Prognosis platform:** Almost always `null`. Only set during active Gleisänderung (track change).

Display logic: show `prognosis.platform` if non-null, else `stop.platform`. Hide if empty.

### passList (Route Stops)

Each stationboard entry includes a `passList` array with all intermediate stops:

```json
"passList": [
  {
    "station": { "id": "8503003", "name": "Zürich Hardbrücke" },
    "arrivalTimestamp": 1718535780,
    "departureTimestamp": 1718535840,
    "delay": 0,
    "platform": "5"
  },
  ...
]
```

**Warning:** Each entry is ~300 bytes. A train with 15 stops = ~4.5KB per departure. On memory-constrained platforms, fetch on demand rather than upfront. Bildschirmli Pebble caches the full response on the phone, then sends just the stop names for one departure when the detail view opens.

---

## Direction Name Sanitizing

Swiss station names follow "City, Stop" format. On small displays, this wastes space.

### Rules (priority order)

```javascript
// 1. "X, Bahnhof" → "Bhf X"
"Zollikon, Bahnhof" → "Bhf Zollikon"

// 2. "Zürich, X" → "X" (Zürich always implied in the region)
"Zürich, Stadelhofen" → "Stadelhofen"

// 3. "<ownCity>, X" → "X" (strip own city prefix)
// If station is in Lufingen: "Lufingen, Bahnhof" → "Bahnhof"

// 4. Generic "City, X" → "X"
"Embrach, Bahnhof" → "Bahnhof"
```

### Common Abbreviations

```javascript
direction = direction
    .replace(/Bahnhof/g, 'Bhf')
    .replace(/Strasse/g, 'Str')
    .replace(/strasse/g, 'str');
```

### Station Name Display (Picker)

When multiple stations share the same city (e.g. all in "Lufingen"):
- **Mode A:** Show stop part only — "BHF", "ROSENWIES.", "AUGWIL"
- **Mode B:** City abbreviation + stop — "LUF BHF", "LUF ROS", "EMB BHF"

Auto-detect: if all stations share `city` prefix, use Mode A. Otherwise Mode B.

---

## URL Encoding

Station names contain umlauts (ü, ö, ä) and spaces. Always URL-encode:

```javascript
// JavaScript (PKJS)
encodeURIComponent(stationName)

// C (ESP8266)
void urlEncode(const char* src, char* dst, size_t maxLen) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j < maxLen - 4; i++) {
        char c = src[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            dst[j++] = c;
        } else {
            snprintf(dst + j, 4, "%%%02X", (unsigned char)c);
            j += 3;
        }
    }
    dst[j] = '\0';
}
```

---

## Rate Limits & Courtesy

The API has no documented rate limit, but:
- Don't poll faster than every 30-60 seconds
- Use field filtering to minimize bandwidth
- Cache results when possible (SmallTV-Ultra caches for 60s)
- The API is community-maintained — don't abuse it

---

## Coverage

The API covers **all Swiss public transport operators**:
- SBB (trains)
- ZVV, BVB, BernMobil, TPG, TL (city transit)
- PostBus
- Regional operators
- Cable cars, boats

Does NOT cover: private buses, cross-border services (except SBB international departures).

---

*Source: transport.opendata.ch, verified during Bildschirmli development (Pebble + SmallTV-Ultra), June 2026*
