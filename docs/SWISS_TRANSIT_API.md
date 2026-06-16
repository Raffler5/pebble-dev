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
    &fields[]=stationboard/stop/departureTimestamp
    &fields[]=stationboard/stop/prognosis/departureTimestamp
```

**On ESP8266 (80KB RAM):** Field filtering is mandatory — 50KB response would crash.
**On Pebble PKJS:** Field filtering is recommended — faster parsing, less phone battery.

### Response (Filtered)

```json
{
  "station": { "name": "Zürich HB" },
  "stationboard": [
    {
      "number": "S2",
      "to": "Ziegelbrücke",
      "stop": {
        "departureTimestamp": 1718535600,
        "prognosis": {
          "departureTimestamp": 1718535660
        }
      }
    }
  ]
}
```

### Departure Time

Prefer realtime prognosis, fall back to scheduled:

```javascript
var depTime = 0;
if (entry.stop.prognosis && entry.stop.prognosis.departureTimestamp) {
    depTime = entry.stop.prognosis.departureTimestamp;  // realtime
} else if (entry.stop.departureTimestamp) {
    depTime = entry.stop.departureTimestamp;  // scheduled
}
var minutes = Math.max(0, Math.floor((depTime - now) / 60));
```

`prognosis.departureTimestamp` is null when no realtime data is available (e.g. late evening, rural routes).

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
