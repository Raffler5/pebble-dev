// Bildschirmli Pebble — Phone-side (PebbleKit JS)
//
// Features:
//   - GPS → find nearby stations → send to watch
//   - Fetch departures for selected station
//   - Settings page (default station, max distance)

var MAX_STATIONS = 6;
var MAX_DEPARTURES = 12;
var DEFAULT_MAX_DISTANCE = 800;

// ── Settings (persisted in localStorage) ─────────────────────

function loadSettings() {
    var raw = localStorage.getItem('bildschirmli-settings');
    if (raw) {
        try { return JSON.parse(raw); } catch(e) {}
    }
    return { defaultStation: '', maxDistance: DEFAULT_MAX_DISTANCE };
}

function saveSettings(s) {
    localStorage.setItem('bildschirmli-settings', JSON.stringify(s));
}

// ── Configuration page (embedded HTML) ───────────────────────

function openConfigPage() {
    var settings = loadSettings();

    var html = '<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width">' +
        '<style>' +
        'body{font-family:-apple-system,Helvetica,sans-serif;background:#1a1a1a;color:#e0e0e0;' +
        'margin:0;padding:20px;font-size:16px}' +
        'h1{color:#ffaa00;font-size:20px;margin:0 0 20px}' +
        'label{display:block;margin:16px 0 6px;color:#ffaa00;font-size:14px}' +
        'input[type=text]{width:100%;box-sizing:border-box;padding:10px;background:#2a2a2a;' +
        'color:#e0e0e0;border:1px solid #ffaa00;border-radius:4px;font-size:16px}' +
        'input[type=range]{width:100%;margin:8px 0}' +
        '.val{color:#ffaa00;font-size:14px}' +
        '.desc{color:#888;font-size:12px;margin:4px 0 0}' +
        'button{width:100%;padding:14px;margin:24px 0;background:#ffaa00;color:#1a1a1a;' +
        'border:none;border-radius:4px;font-size:18px;font-weight:bold;cursor:pointer}' +
        'button:active{background:#cc8800}' +
        '</style></head><body>' +
        '<h1>Bildschirmli Settings</h1>' +

        '<label>Default Station</label>' +
        '<input type="text" id="station" value="' + (settings.defaultStation || '') + '" ' +
        'placeholder="e.g. Bern, Bahnhof">' +
        '<p class="desc">Always show this station in the picker. Leave empty for GPS only.</p>' +

        '<label>Max Distance: <span class="val" id="distval">' + settings.maxDistance + 'm</span></label>' +
        '<input type="range" id="distance" min="200" max="2000" step="100" ' +
        'value="' + settings.maxDistance + '" ' +
        'oninput="document.getElementById(\'distval\').textContent=this.value+\'m\'">' +
        '<p class="desc">Search radius for nearby stations (200m – 2000m)</p>' +

        '<button onclick="submit()">Save</button>' +

        '<script>' +
        'function submit(){' +
        'var s=JSON.stringify({' +
        'defaultStation:document.getElementById("station").value,' +
        'maxDistance:parseInt(document.getElementById("distance").value)' +
        '});' +
        'var loc="pebblejs://close#"+encodeURIComponent(s);' +
        'document.location.href=loc;}' +
        '</script>' +
        '</body></html>';

    Pebble.openURL('data:text/html,' + encodeURIComponent(html));
}

// ── Helpers ──────────────────────────────────────────────────

function xhrGet(url, callback) {
    var xhr = new XMLHttpRequest();
    xhr.onload = function() { callback(null, this.responseText); };
    xhr.onerror = function() { callback('XHR error', null); };
    xhr.ontimeout = function() { callback('Timeout', null); };
    xhr.timeout = 15000;
    xhr.open('GET', url);
    xhr.send();
}

function haversineM(lat1, lon1, lat2, lon2) {
    var R = 6371000;
    var dLat = (lat2 - lat1) * Math.PI / 180;
    var dLon = (lon2 - lon1) * Math.PI / 180;
    var a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
            Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) *
            Math.sin(dLon / 2) * Math.sin(dLon / 2);
    return R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
}

function splitStationName(fullName) {
    var comma = fullName.indexOf(', ');
    var city, stop;
    if (comma > 0) {
        city = fullName.substring(0, comma);
        stop = fullName.substring(comma + 2);
    } else {
        city = fullName;
        stop = '';
    }
    stop = stop.replace(/Bahnhof/g, 'Bhf').replace(/Strasse/g, 'Str').replace(/strasse/g, 'str');
    return { city: city, stop: stop };
}

function sanitizeDirection(stationFull, rawTo) {
    if (!rawTo) return '';
    var bhfIdx = rawTo.indexOf(', Bahnhof');
    if (bhfIdx > 0) return 'Bhf ' + rawTo.substring(0, bhfIdx);
    if (rawTo.indexOf('Zürich, ') === 0) return rawTo.substring(8);
    if (stationFull) {
        var comma = stationFull.indexOf(',');
        if (comma > 0) {
            var city = stationFull.substring(0, comma);
            if (rawTo.indexOf(city + ', ') === 0) return rawTo.substring(city.length + 2);
        }
    }
    var c = rawTo.indexOf(', ');
    if (c > 0 && c < 20) return rawTo.substring(c + 2);
    return rawTo;
}

// ── Format and send station list to watch ────────────────────

function sendStationsToWatch(entries) {
    entries.sort(function(a, b) { return a.distance - b.distance; });
    var count = Math.min(entries.length, MAX_STATIONS);
    if (count === 0) {
        Pebble.sendAppMessage({ 'KEY_ERROR': 'No stops found' });
        return;
    }

    var result = '';
    for (var j = 0; j < count; j++) {
        result += entries[j].city + '|' + entries[j].stop + ':' +
                  entries[j].id + ':' + entries[j].distance + ';';
    }
    result += '%';

    console.log('Bildschirmli: sending ' + count + ' stations');
    Pebble.sendAppMessage({ 'KEY_STATION': result });
}

// ── Phase 1: Find stations ───────────────────────────────────

var s_userLat = 0;
var s_userLon = 0;

function findStations() {
    var settings = loadSettings();
    var maxDist = settings.maxDistance || DEFAULT_MAX_DISTANCE;
    var defaultStation = settings.defaultStation || '';

    // If default station is configured, search by name AND by GPS
    if (defaultStation) {
        // First fetch the default station by name
        var nameUrl = 'http://transport.opendata.ch/v1/locations?query=' +
                      encodeURIComponent(defaultStation) +
                      '&type=station&limit=1';

        xhrGet(nameUrl, function(err, resp) {
            var defaultEntries = [];
            if (!err && resp) {
                try {
                    var json = JSON.parse(resp);
                    if (json.stations && json.stations.length > 0) {
                        var st = json.stations[0];
                        var parts = splitStationName(st.name);
                        defaultEntries.push({
                            city: parts.city, stop: parts.stop,
                            id: st.id, distance: 0  // distance 0 = pinned
                        });
                    }
                } catch(e) {}
            }

            // Then also find GPS-nearby stations
            findGPSStations(maxDist, function(gpsEntries) {
                // Merge: default station first, then GPS (dedup by ID)
                var merged = defaultEntries.slice();
                for (var i = 0; i < gpsEntries.length; i++) {
                    var isDup = false;
                    for (var j = 0; j < merged.length; j++) {
                        if (merged[j].id === gpsEntries[i].id) { isDup = true; break; }
                    }
                    if (!isDup) merged.push(gpsEntries[i]);
                }
                sendStationsToWatch(merged);
            });
        });
    } else {
        // GPS only
        findGPSStations(maxDist, function(entries) {
            sendStationsToWatch(entries);
        });
    }
}

function findGPSStations(maxDist, callback) {
    navigator.geolocation.getCurrentPosition(
        function(pos) {
            s_userLat = pos.coords.latitude;
            s_userLon = pos.coords.longitude;

            var url = 'http://transport.opendata.ch/v1/locations?x=' +
                      s_userLat + '&y=' + s_userLon +
                      '&type=station&limit=15';

            xhrGet(url, function(err, responseText) {
                if (err || !responseText) {
                    callback([]);
                    return;
                }
                var json;
                try { json = JSON.parse(responseText); } catch(e) { callback([]); return; }
                if (!json.stations) { callback([]); return; }

                var entries = [];
                for (var i = 0; i < json.stations.length; i++) {
                    var st = json.stations[i];
                    if (!st.id || !st.name) continue;
                    var dist = 9999;
                    if (st.coordinate && st.coordinate.x && st.coordinate.y) {
                        dist = Math.round(haversineM(
                            s_userLat, s_userLon,
                            st.coordinate.x, st.coordinate.y));
                    }
                    if (dist > maxDist) continue;
                    var parts = splitStationName(st.name);
                    entries.push({ city: parts.city, stop: parts.stop, id: st.id, distance: dist });
                }
                callback(entries);
            });
        },
        function(err) {
            console.log('Bildschirmli: GPS error: ' + err.message);
            Pebble.sendAppMessage({ 'KEY_ERROR': 'GPS unavailable' });
            callback([]);
        },
        { timeout: 15000, maximumAge: 60000 }
    );
}

// ── Phase 2: Fetch departures ────────────────────────────────

function fetchDepartures(stationId) {
    var url = 'http://transport.opendata.ch/v1/stationboard?id=' + stationId +
              '&limit=' + MAX_DEPARTURES +
              '&fields[]=station/name' +
              '&fields[]=stationboard/to' +
              '&fields[]=stationboard/number' +
              '&fields[]=stationboard/stop/departureTimestamp' +
              '&fields[]=stationboard/stop/prognosis/departureTimestamp';

    xhrGet(url, function(err, responseText) {
        if (err) { Pebble.sendAppMessage({ 'KEY_ERROR': 'API: ' + err }); return; }
        var json;
        try { json = JSON.parse(responseText); } catch(e) {
            Pebble.sendAppMessage({ 'KEY_ERROR': 'JSON error' }); return;
        }
        if (!json.station || !json.stationboard) {
            Pebble.sendAppMessage({ 'KEY_ERROR': 'Bad response' }); return;
        }

        var stationName = json.station.name || '?';
        var parts = splitStationName(stationName);
        var headerName = parts.stop || parts.city;
        var now = Math.floor(Date.now() / 1000);

        var result = headerName + ';';
        var count = Math.min(json.stationboard.length, MAX_DEPARTURES);
        for (var i = 0; i < count; i++) {
            var entry = json.stationboard[i];
            var line = entry.number || '?';
            var dir = sanitizeDirection(stationName, entry.to);
            dir = dir.replace(/Bahnhof/g, 'Bhf').replace(/Strasse/g, 'Str').replace(/strasse/g, 'str');

            var depTime = 0;
            if (entry.stop) {
                if (entry.stop.prognosis && entry.stop.prognosis.departureTimestamp)
                    depTime = entry.stop.prognosis.departureTimestamp;
                else if (entry.stop.departureTimestamp)
                    depTime = entry.stop.departureTimestamp;
            }
            var minutes = depTime > 0 ? Math.max(0, Math.floor((depTime - now) / 60)) : 99;
            result += line + ':' + dir + ':' + minutes + ';';
        }
        result += '%';

        Pebble.sendAppMessage({ 'KEY_DEPARTURES': result });
    });
}

// ── Event handlers ───────────────────────────────────────────

Pebble.addEventListener('ready', function(e) {
    console.log('Bildschirmli: PebbleKit JS ready');
    findStations();
});

Pebble.addEventListener('appmessage', function(e) {
    if (e.payload.KEY_FETCH) fetchDepartures('' + e.payload.KEY_FETCH);
    if (e.payload.KEY_STATION) findStations();
});

// Settings page — gear icon in Pebble app
Pebble.addEventListener('showConfiguration', function(e) {
    openConfigPage();
});

Pebble.addEventListener('webviewclosed', function(e) {
    if (!e.response) return;
    try {
        var settings = JSON.parse(decodeURIComponent(e.response));
        saveSettings(settings);
        console.log('Bildschirmli: settings saved: ' + JSON.stringify(settings));

        // Send settings to watch for persistent storage
        Pebble.sendAppMessage({
            'KEY_CFG_DEFAULT_STATION': settings.defaultStation || '',
            'KEY_CFG_MAX_DISTANCE': settings.maxDistance || DEFAULT_MAX_DISTANCE
        });

        // Refresh stations with new settings
        findStations();
    } catch(ex) {
        console.log('Bildschirmli: settings parse error: ' + ex);
    }
});
