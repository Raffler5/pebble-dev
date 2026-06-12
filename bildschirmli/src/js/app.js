// Bildschirmli Pebble — Phone-side (PebbleKit JS)
//
// Two-phase communication:
//   1. GPS → find nearest stations → send station list to watch
//   2. Watch requests departures for a specific station ID → fetch → send
//
// API: transport.opendata.ch

var MAX_STATIONS = 6;
var MAX_DEPARTURES = 12;
var MAX_DISTANCE_M = 800;

function xhrGet(url, callback) {
    var xhr = new XMLHttpRequest();
    xhr.onload = function() { callback(null, this.responseText); };
    xhr.onerror = function() { callback('XHR error', null); };
    xhr.ontimeout = function() { callback('Timeout', null); };
    xhr.timeout = 15000;
    xhr.open('GET', url);
    xhr.send();
}

// Haversine distance in meters
function haversineM(lat1, lon1, lat2, lon2) {
    var R = 6371000;
    var dLat = (lat2 - lat1) * Math.PI / 180;
    var dLon = (lon2 - lon1) * Math.PI / 180;
    var a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
            Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) *
            Math.sin(dLon / 2) * Math.sin(dLon / 2);
    return R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
}

// Split "City, Stop" into { city: "City", stop: "Stop" }
// Abbreviate common words in the stop part
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

    // Abbreviate common words in stop
    stop = stop.replace(/Bahnhof/g, 'Bhf')
               .replace(/Strasse/g, 'Str')
               .replace(/strasse/g, 'str');

    return { city: city, stop: stop };
}

// Direction sanitizer for departure view
function sanitizeDirection(stationFull, rawTo) {
    if (!rawTo) return '';

    // "X, Bahnhof" → "Bhf. X"
    var bhfIdx = rawTo.indexOf(', Bahnhof');
    if (bhfIdx > 0) return 'Bhf ' + rawTo.substring(0, bhfIdx);

    // "Zürich, X" → "X"
    if (rawTo.indexOf('Zürich, ') === 0) return rawTo.substring(8);

    // "<ownCity>, X" → "X"
    if (stationFull) {
        var comma = stationFull.indexOf(',');
        if (comma > 0) {
            var city = stationFull.substring(0, comma);
            if (rawTo.indexOf(city + ', ') === 0) return rawTo.substring(city.length + 2);
        }
    }

    // Generic "City, X" → "X"
    var c = rawTo.indexOf(', ');
    if (c > 0 && c < 20) return rawTo.substring(c + 2);

    return rawTo;
}

// ── Phase 1: Find nearby stations ────────────────────────────

var s_userLat = 0;
var s_userLon = 0;

function findStations() {
    navigator.geolocation.getCurrentPosition(
        function(pos) {
            s_userLat = pos.coords.latitude;
            s_userLon = pos.coords.longitude;

            // Request more than MAX_STATIONS since we'll filter by distance
            var url = 'http://transport.opendata.ch/v1/locations?x=' +
                      s_userLat + '&y=' + s_userLon +
                      '&type=station&limit=15';

            console.log('Bildschirmli: finding stations near ' +
                        s_userLat.toFixed(4) + ',' + s_userLon.toFixed(4));

            xhrGet(url, function(err, responseText) {
                if (err) {
                    Pebble.sendAppMessage({ 'KEY_ERROR': 'Station: ' + err });
                    return;
                }

                var json;
                try { json = JSON.parse(responseText); }
                catch(e) {
                    Pebble.sendAppMessage({ 'KEY_ERROR': 'JSON error' });
                    return;
                }

                if (!json.stations || json.stations.length === 0) {
                    Pebble.sendAppMessage({ 'KEY_ERROR': 'No stations' });
                    return;
                }

                // Build entries with distance, filter, sort
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

                    if (dist > MAX_DISTANCE_M) continue;

                    var parts = splitStationName(st.name);
                    entries.push({
                        city: parts.city,
                        stop: parts.stop,
                        id: st.id,
                        distance: dist
                    });
                }

                entries.sort(function(a, b) { return a.distance - b.distance; });

                var count = Math.min(entries.length, MAX_STATIONS);
                if (count === 0) {
                    Pebble.sendAppMessage({ 'KEY_ERROR': 'No stops <800m' });
                    return;
                }

                // Format: "city1|stop1:id1:dist1;city2|stop2:id2:dist2;...%"
                // Use | as city/stop separator (: is already used for fields)
                var result = '';
                for (var j = 0; j < count; j++) {
                    result += entries[j].city + '|' +
                              entries[j].stop + ':' +
                              entries[j].id + ':' +
                              entries[j].distance + ';';
                }
                result += '%';

                console.log('Bildschirmli: sending ' + count + ' stations (' +
                            result.length + 'B)');

                Pebble.sendAppMessage({
                    'KEY_STATION': result
                }, function() {
                    console.log('Bildschirmli: stations sent OK');
                }, function() {
                    console.log('Bildschirmli: stations send failed');
                });
            });
        },
        function(err) {
            console.log('Bildschirmli: GPS error: ' + err.message);
            Pebble.sendAppMessage({ 'KEY_ERROR': 'GPS unavailable' });
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

    console.log('Bildschirmli: fetching departures for ' + stationId);

    xhrGet(url, function(err, responseText) {
        if (err) {
            Pebble.sendAppMessage({ 'KEY_ERROR': 'API: ' + err });
            return;
        }

        var json;
        try { json = JSON.parse(responseText); }
        catch(e) {
            Pebble.sendAppMessage({ 'KEY_ERROR': 'JSON error' });
            return;
        }

        if (!json.station || !json.stationboard) {
            Pebble.sendAppMessage({ 'KEY_ERROR': 'Bad response' });
            return;
        }

        var stationName = json.station.name || '?';
        // Send the stop part for the header (after comma), abbreviated
        var parts = splitStationName(stationName);
        var headerName = parts.stop || parts.city;

        var now = Math.floor(Date.now() / 1000);

        // Format: "HeaderName;line:dir:min;...%"
        var result = headerName + ';';
        var count = Math.min(json.stationboard.length, MAX_DEPARTURES);

        for (var i = 0; i < count; i++) {
            var entry = json.stationboard[i];
            var line = entry.number || '?';
            var dir = sanitizeDirection(stationName, entry.to);

            // Abbreviate common direction words
            dir = dir.replace(/Bahnhof/g, 'Bhf')
                     .replace(/Strasse/g, 'Str')
                     .replace(/strasse/g, 'str');

            var depTime = 0;
            if (entry.stop) {
                if (entry.stop.prognosis && entry.stop.prognosis.departureTimestamp) {
                    depTime = entry.stop.prognosis.departureTimestamp;
                } else if (entry.stop.departureTimestamp) {
                    depTime = entry.stop.departureTimestamp;
                }
            }

            var minutes = depTime > 0 ? Math.max(0, Math.floor((depTime - now) / 60)) : 99;
            result += line + ':' + dir + ':' + minutes + ';';
        }
        result += '%';

        Pebble.sendAppMessage({
            'KEY_DEPARTURES': result
        }, function() {
            console.log('Bildschirmli: departures sent OK');
        }, function() {
            console.log('Bildschirmli: departures send failed');
        });
    });
}

// ── Event handlers ───────────────────────────────────────────

Pebble.addEventListener('ready', function(e) {
    console.log('Bildschirmli: PebbleKit JS ready');
    findStations();
});

Pebble.addEventListener('appmessage', function(e) {
    console.log('Bildschirmli: appmessage: ' + JSON.stringify(e.payload));

    if (e.payload.KEY_FETCH) {
        fetchDepartures('' + e.payload.KEY_FETCH);
    }
    if (e.payload.KEY_STATION) {
        findStations();
    }
});
