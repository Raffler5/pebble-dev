# Pebble Settings Page Patterns

How to add a phone-side configuration page to a Pebble app.

---

## Two Approaches

### Clay (Recommended for complex settings)

Clay is a JS library that generates a config page from JSON. No HTML needed.

- **v1.x (`@rebble/clay`):** Works with `package.json` format. CloudPebble has it as a built-in NPM dependency.
- **v0.1.7:** Works with `appinfo.json` + `appKeys`. Requires manual `clay.js` download.

Best for: color pickers, toggles, many settings, standard form layouts.

### Manual HTML (Better for custom/interactive settings)

Embed an HTML page as a `data:text/html` URI. The page runs on the phone with full access to XHR, localStorage, and the DOM.

Best for: station search with live API queries, custom UIs, few settings that need interactive behavior.

**This is what Bildschirmli uses** — the station search requires live API calls from the settings page, which Clay can't do.

---

## Manual Settings Page — Implementation

### 1. Enable in appinfo.json

```json
{
  "capabilities": ["location", "configurable"],
  "appKeys": {
    "KEY_CFG_MY_SETTING": 4
  }
}
```

The `"configurable"` capability makes the gear icon appear next to the app in the Pebble phone app.

### 2. Build the HTML page in JS

**Important:** Pebble's phone webview ignores `charset=utf-8` on data URIs. Any non-ASCII text (umlauts, accents) embedded in the HTML template **must** be escaped to `\uXXXX` JS sequences. See COMMON_PITFALLS.md §9 for the full story.

```javascript
function openConfigPage() {
    var settings = loadSettings();

    // Escape non-ASCII to \uXXXX so HTML stays pure ASCII
    var dataJSON = JSON.stringify(settings.data).replace(
        /[\u0080-\uffff]/g, function(c) {
            return '\\u' + ('0000' + c.charCodeAt(0).toString(16)).slice(-4);
        });

    var html = '<!DOCTYPE html><html><head>' +
        '<meta name="viewport" content="width=device-width">' +
        '<style>/* your CSS */</style>' +
        '</head><body>' +
        '<!-- your form -->' +
        '<button onclick="submit()">Save</button>' +
        '<script>' +
        'function submit(){' +
        'var data = JSON.stringify({ /* your settings */ });' +
        'document.location.href = "pebblejs://close#" + encodeURIComponent(data);' +
        '}' +
        '</script></body></html>';

    Pebble.openURL('data:text/html,' + encodeURIComponent(html));
}
```

### 3. Handle the events

```javascript
Pebble.addEventListener('showConfiguration', function(e) {
    openConfigPage();
});

Pebble.addEventListener('webviewclosed', function(e) {
    if (!e.response) return;
    try {
        var settings = JSON.parse(decodeURIComponent(e.response));
        saveSettings(settings);
        // Optionally send to watch via AppMessage
        // Optionally refresh data
    } catch(ex) {}
});
```

### 4. Persist on phone (localStorage)

```javascript
function loadSettings() {
    try { return JSON.parse(localStorage.getItem('my-settings')) || {}; }
    catch(e) { return {}; }
}

function saveSettings(s) {
    localStorage.setItem('my-settings', JSON.stringify(s));
}
```

### 5. Optionally persist on watch (C side)

```c
#define SETTINGS_PKEY 1

typedef struct {
    int my_value;
} UserSettings;

static UserSettings s_settings;

static void settings_load(void) {
    s_settings.my_value = 42;  // default
    if (persist_exists(SETTINGS_PKEY)) {
        persist_read_data(SETTINGS_PKEY, &s_settings, sizeof(s_settings));
    }
}

static void settings_save(void) {
    persist_write_data(SETTINGS_PKEY, &s_settings, sizeof(s_settings));
}
```

---

## Lessons from Bildschirmli Station Search

### Problem: Escaping station names in onclick handlers

**Wrong:** Injecting station names directly into `onclick="addFav('Zürich, HB')"` — breaks with quotes, commas, umlauts.

**Right:** Store search results in an array, reference by index:
```javascript
var searchResults = [];

// In search callback:
searchResults.push({ name: s.name, id: s.id });
html += '<div onclick="addFav(' + (searchResults.length - 1) + ')">';

// Handler uses index:
function addFav(idx) {
    var s = searchResults[idx];
    // safe — no string escaping needed
}
```

### Problem: Same-name stations

Some stops share names (e.g. "Schmiede Wiedikon" has two — one per tram direction). The transport.opendata.ch API returns both with different IDs but the same name.

**Fix:** Show coordinates after the name, like SmallTV-Ultra does:
```javascript
var label = station.name;
if (station.coordinate && station.coordinate.x) {
    label += ' (' + station.coordinate.x.toFixed(3) + ', ' +
             station.coordinate.y.toFixed(3) + ')';
}
```

### Problem: User types garbage

If the user types "asdf" and saves it as a favorite, the API won't find it later when resolving favorites.

**Fix:** The settings page searches the API live — the user can only add stations that the API actually returned. The favorite stores `{name, id}` where `id` is the API's official station ID. When loading favorites at app start, resolve by ID (not name).

### Minimum query length

Require at least 2 characters before searching. Single-character queries return too many results and are slow.

```javascript
if (q.length < 2) {
    showMessage('Enter at least 2 characters');
    return;
}
```

### API endpoint for station search

```
http://transport.opendata.ch/v1/locations?query={query}&type=station
```

- No `&limit=` parameter needed — show all results
- `&type=station` filters out addresses and POIs
- Returns: `{ stations: [{ id, name, coordinate: { x, y } }] }`

### Search UX feedback (from SmallTV-Ultra)

- Button text changes to "..." while searching
- Status message: "Searching..." → "5 found" or "None found"
- Disable the search button during fetch to prevent double-clicks
- Re-enable on success, error, or timeout

### HTML escaping

Always escape user-visible text to prevent XSS (station names can contain `<`, `&`, quotes):
```javascript
function esc(s) {
    return s.replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;');
}
```

---

## AppMessage Sequencing After Config Save

When `webviewclosed` fires, you often need to both send settings to the watch AND refresh app data. Both use `sendAppMessage`, but PebbleKit JS only allows one message in flight at a time. **Always chain via callbacks:**

```javascript
Pebble.addEventListener('webviewclosed', function(e) {
    if (!e.response) return;
    var settings = JSON.parse(decodeURIComponent(e.response));
    saveSettings(settings);

    // Send settings to watch, THEN refresh data
    var msg = {};
    if (settings.color) msg['KEY_CFG_COLOR'] = settings.color;
    if (settings.bgColor) msg['KEY_CFG_BG_COLOR'] = settings.bgColor;

    if (Object.keys(msg).length > 0) {
        Pebble.sendAppMessage(msg, function() {
            refreshData();  // only after ACK
        }, function() {
            refreshData();  // also on error
        });
    } else {
        refreshData();
    }
});
```

See COMMON_PITFALLS.md §8 for details on why this is necessary.

---

## Data Flow Summary

```
User taps gear icon
    → Pebble fires 'showConfiguration'
    → JS opens data:text/html;charset=utf-8 page with current settings pre-filled
    → User modifies settings, taps Save
    → Page navigates to pebblejs://close#{encoded JSON}
    → Pebble fires 'webviewclosed' with e.response = encoded JSON
    → JS parses, saves to localStorage
    → JS sends settings to watch via AppMessage (wait for ACK)
    → JS refreshes app data with new settings (in sendAppMessage callback)
```

---

*Learned from: Bildschirmli Pebble + SmallTV-Ultra station search implementation*
*Date: 2026-06-16*
