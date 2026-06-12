// Bildschirmli Pebble — Phone-side (PebbleKit JS)
//
// GPS → nearest station → fetch departures → send to watch.
// Based on Tramlin's pattern + Bildschirmli's field filtering and
// direction sanitizing.

var MAX_DEPARTURES = 8;

function xhrGet(url, callback) {
    var xhr = new XMLHttpRequest();
    xhr.onload = function() { callback(null, this.responseText); };
    xhr.onerror = function() { callback('XHR error', null); };
    xhr.ontimeout = function() { callback('XHR timeout', null); };
    xhr.timeout = 15000;
    xhr.open('GET', url);
    xhr.send();
}

// Strip redundant prefixes from direction names.
// "Zürich, Albisgütli" → "Albisgütli"
// "Zollikon, Bahnhof"  → "Bhf. Zollikon"
// "<ownCity>, X"        → "X"
function sanitizeDirection(stationFull, rawTo) {
    if (!rawTo) return '';

    // "X, Bahnhof" → "Bhf. X"
    var bhfIdx = rawTo.indexOf(', Bahnhof');
    if (bhfIdx > 0) {
        return 'Bhf. ' + rawTo.substring(0, bhfIdx);
    }

    // "Zürich, X" → "X" (always implied in Zürich region)
    if (rawTo.indexOf('Zürich, ') === 0) {
        return rawTo.substring(8);
    }

    // "<ownCity>, X" → "X"
    if (stationFull) {
        var comma = stationFull.indexOf(',');
        if (comma > 0) {
            var city = stationFull.substring(0, comma);
            if (rawTo.indexOf(city + ', ') === 0) {
                return rawTo.substring(city.length + 2);
            }
        }
    }

    // Strip any leading "city, " for compactness
    var c = rawTo.indexOf(', ');
    if (c > 0 && c < 20) {
        return rawTo.substring(c + 2);
    }

    return rawTo;
}

function fetchDepartures(stationId) {
    // Field filtering — cuts response from ~50KB to ~2KB
    var url = 'http://transport.opendata.ch/v1/stationboard?id=' + stationId +
              '&limit=' + MAX_DEPARTURES +
              '&fields[]=station/name' +
              '&fields[]=stationboard/to' +
              '&fields[]=stationboard/number' +
              '&fields[]=stationboard/stop/departureTimestamp' +
              '&fields[]=stationboard/stop/prognosis/departureTimestamp';

    console.log('Bildschirmli: fetching ' + url);

    xhrGet(url, function(err, responseText) {
        if (err || !responseText) {
            console.log('Bildschirmli: fetch error: ' + err);
            Pebble.sendAppMessage({ 'KEY_ERROR': err || 'unknown error' });
            return;
        }

        var json;
        try { json = JSON.parse(responseText); }
        catch(e) {
            Pebble.sendAppMessage({ 'KEY_ERROR': 'JSON parse error' });
            return;
        }

        if (!json.station || !json.stationboard) {
            Pebble.sendAppMessage({ 'KEY_ERROR': 'Bad API response' });
            return;
        }

        var stationName = json.station.name || '?';
        var now = Math.floor(Date.now() / 1000);

        // Build compact departure string:
        // "StationName;line1:dir1:min1;line2:dir2:min2;...%"
        var result = stationName + ';';

        var count = Math.min(json.stationboard.length, MAX_DEPARTURES);
        for (var i = 0; i < count; i++) {
            var entry = json.stationboard[i];
            var line = entry.number || '?';
            var dir = sanitizeDirection(stationName, entry.to);

            // Prefer realtime prognosis, fall back to scheduled
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

        console.log('Bildschirmli: sending ' + result.length + ' bytes');

        Pebble.sendAppMessage({
            'KEY_DEPARTURES': result
        }, function() {
            console.log('Bildschirmli: sent OK');
        }, function(e) {
            console.log('Bildschirmli: send failed');
        });
    });
}

function findNearestStation() {
    navigator.geolocation.getCurrentPosition(
        function(pos) {
            var url = 'http://transport.opendata.ch/v1/locations?x=' +
                      pos.coords.latitude + '&y=' + pos.coords.longitude +
                      '&type=station&limit=1';

            console.log('Bildschirmli: finding station at ' +
                        pos.coords.latitude + ',' + pos.coords.longitude);

            xhrGet(url, function(err, responseText) {
                if (err || !responseText) {
                    Pebble.sendAppMessage({ 'KEY_ERROR': 'Location API error' });
                    return;
                }

                var json;
                try { json = JSON.parse(responseText); }
                catch(e) {
                    Pebble.sendAppMessage({ 'KEY_ERROR': 'Location JSON error' });
                    return;
                }

                if (!json.stations || json.stations.length === 0) {
                    Pebble.sendAppMessage({ 'KEY_ERROR': 'No stations found' });
                    return;
                }

                var station = json.stations[0];
                console.log('Bildschirmli: nearest station = ' + station.name +
                            ' (id=' + station.id + ')');

                fetchDepartures(station.id);
            });
        },
        function(err) {
            console.log('Bildschirmli: GPS error: ' + err.message);
            Pebble.sendAppMessage({ 'KEY_ERROR': 'GPS unavailable' });
        },
        { timeout: 15000, maximumAge: 60000 }
    );
}

// Ready — fetch on launch
Pebble.addEventListener('ready', function(e) {
    console.log('Bildschirmli: PebbleKit JS ready');
    findNearestStation();
});

// Watch can request a refresh
Pebble.addEventListener('appmessage', function(e) {
    console.log('Bildschirmli: appmessage received');
    if (e.payload.KEY_FETCH) {
        findNearestStation();
    }
});
