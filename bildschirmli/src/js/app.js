// Bildschirmli Pebble — Phone-side (PebbleKit JS)
//
// Features:
//   - GPS → find nearby stations → send to watch
//   - Fetch departures for selected station
//   - Settings page with station search + multiple favorites

var MAX_STATIONS = 10;
var MAX_DEPARTURES = 12;
var DEFAULT_MAX_DISTANCE = 800;

// ── Settings ─────────────────────────────────────────────────

function loadSettings() {
    var raw = localStorage.getItem('bildschirmli-settings');
    if (raw) {
        try {
            var s = JSON.parse(raw);
            // Migration from v1 single station to v2 array
            if (s.defaultStation && !s.favorites) {
                s.favorites = [{ name: s.defaultStation, id: '' }];
                delete s.defaultStation;
            }
            return s;
        } catch(e) {}
    }
    return { favorites: [], maxDistance: DEFAULT_MAX_DISTANCE };
}

function saveSettings(s) {
    localStorage.setItem('bildschirmli-settings', JSON.stringify(s));
}

// ── Configuration page ───────────────────────────────────────

function openConfigPage() {
    var settings = loadSettings();
    var favsJSON = JSON.stringify(settings.favorites || []);

    var html = '<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width">' +
        '<style>' +
        '*{box-sizing:border-box}' +
        'body{font-family:-apple-system,Helvetica,sans-serif;background:#1a1a1a;color:#e0e0e0;' +
        'margin:0;padding:16px;font-size:15px}' +
        'h1{color:#ffaa00;font-size:20px;margin:0 0 16px}' +
        'h2{color:#ffaa00;font-size:14px;margin:16px 0 8px;text-transform:uppercase;letter-spacing:1px}' +
        'input[type=text]{width:100%;padding:10px;background:#2a2a2a;' +
        'color:#e0e0e0;border:1px solid #555;border-radius:4px;font-size:15px}' +
        'input[type=text]:focus{border-color:#ffaa00;outline:none}' +
        'input[type=range]{width:100%;margin:8px 0}' +
        '.val{color:#ffaa00}' +
        '.desc{color:#888;font-size:12px;margin:4px 0 0}' +

        '.fav{display:flex;align-items:center;padding:8px;margin:4px 0;' +
        'background:#2a2a2a;border-radius:4px;border-left:3px solid #ffaa00}' +
        '.fav .name{flex:1;color:#e0e0e0}' +
        '.fav .id{color:#888;font-size:11px;margin-left:4px}' +
        '.fav .rm{color:#ff4444;cursor:pointer;padding:4px 8px;font-size:18px;' +
        'background:none;border:none}' +
        '.empty{color:#666;font-style:italic;padding:8px}' +

        '.search-box{display:flex;gap:8px;margin:8px 0}' +
        '.search-box input{flex:1}' +
        '.search-btn{padding:10px 16px;background:#ffaa00;color:#1a1a1a;border:none;' +
        'border-radius:4px;font-weight:bold;cursor:pointer;white-space:nowrap}' +
        '.search-btn:active{background:#cc8800}' +

        '.result{display:flex;align-items:center;padding:8px;margin:4px 0;' +
        'background:#222;border-radius:4px;cursor:pointer;border:1px solid #333}' +
        '.result:active{background:#333;border-color:#ffaa00}' +
        '.result .name{flex:1}' +
        '.result .add{color:#ffaa00;font-size:20px;padding:0 8px}' +
        '.searching{color:#888;padding:8px;font-style:italic}' +

        'button.save{width:100%;padding:14px;margin:20px 0;background:#ffaa00;color:#1a1a1a;' +
        'border:none;border-radius:4px;font-size:18px;font-weight:bold;cursor:pointer}' +
        'button.save:active{background:#cc8800}' +
        '</style></head><body>' +

        '<h1>Bildschirmli</h1>' +

        // Favorite stations
        '<h2>Favorite Stations</h2>' +
        '<div id="favs"></div>' +

        // Search
        '<h2>Add Station</h2>' +
        '<div class="search-box">' +
        '<input type="text" id="q" placeholder="Search station...">' +
        '<button class="search-btn" onclick="search()">Find</button>' +
        '</div>' +
        '<div id="results"></div>' +
        '<p class="desc">Search by name (e.g. "Bern Bahnhof", "Stettbach"). Tap a result to add it to favorites.</p>' +

        // Max distance
        '<h2>Nearby Search Radius</h2>' +
        '<p>Max distance: <span class="val" id="distval">' + settings.maxDistance + 'm</span></p>' +
        '<input type="range" id="distance" min="200" max="2000" step="100" ' +
        'value="' + settings.maxDistance + '" ' +
        'oninput="document.getElementById(\'distval\').textContent=this.value+\'m\'">' +
        '<p class="desc">GPS stations beyond this distance are hidden (200m – 2000m)</p>' +

        // Save
        '<button class="save" onclick="submit()">Save Settings</button>' +

        '<script>' +
        'var favs=' + favsJSON + ';' +
        // Store search results so we can reference by index (avoids escaping issues)
        'var searchResults=[];' +

        'function renderFavs(){' +
        'var el=document.getElementById("favs");' +
        'if(!favs.length){el.innerHTML="<div class=\\"empty\\">No favorites yet</div>";return;}' +
        'var h="";' +
        'for(var i=0;i<favs.length;i++){' +
        'h+="<div class=\\"fav\\"><span class=\\"name\\">"+esc(favs[i].name)+"</span>";' +
        'h+="<button class=\\"rm\\" onclick=\\"rmFav("+i+")\\">&times;</button></div>";}' +
        'el.innerHTML=h;}' +

        'function esc(s){return s.replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;");}' +

        'function rmFav(i){favs.splice(i,1);renderFavs();}' +

        // Add by index into searchResults — avoids all escaping issues
        'function addFav(idx){' +
        'var s=searchResults[idx];if(!s)return;' +
        'for(var i=0;i<favs.length;i++){if(favs[i].id===s.id)return;}' +
        'favs.push({name:s.name,id:s.id});renderFavs();' +
        'document.getElementById("results").innerHTML="<div class=\\"empty\\">Added!</div>";}' +

        'function search(){' +
        'var q=document.getElementById("q").value.trim();' +
        'if(q.length<2){document.getElementById("results").innerHTML=' +
        '"<div class=\\"empty\\">Enter at least 2 characters</div>";return;}' +
        'var btn=document.querySelector(".search-btn");' +
        'btn.disabled=true;btn.textContent="...";' +
        'document.getElementById("results").innerHTML="<div class=\\"searching\\">Searching...</div>";' +
        'var xhr=new XMLHttpRequest();' +
        'xhr.timeout=10000;' +
        'xhr.onload=function(){' +
        'btn.disabled=false;btn.textContent="Find";' +
        'try{var j=JSON.parse(this.responseText);' +
        'searchResults=[];' +
        'var h="";' +
        'if(!j.stations||!j.stations.length){h="<div class=\\"empty\\">No stations found</div>";}' +
        'else{for(var i=0;i<j.stations.length;i++){' +
        'var s=j.stations[i];if(!s.id||!s.name)continue;' +
        'searchResults.push({name:s.name,id:s.id});' +
        'var label=esc(s.name);' +
        // Show coordinates to disambiguate same-name stops (SmallTV-Ultra pattern)
        'if(s.coordinate&&s.coordinate.x)label+=" <span class=\\"id\\">("+s.coordinate.x.toFixed(3)+", "+s.coordinate.y.toFixed(3)+")</span>";' +
        'h+="<div class=\\"result\\" onclick=\\"addFav("+(searchResults.length-1)+")\\">"+' +
        '"<span class=\\"name\\">"+label+"</span><span class=\\"add\\">+</span></div>";}}' +
        'document.getElementById("results").innerHTML=h;' +
        '}catch(e){document.getElementById("results").innerHTML="<div class=\\"empty\\">Error</div>";}};' +
        'xhr.onerror=function(){btn.disabled=false;btn.textContent="Find";' +
        'document.getElementById("results").innerHTML="<div class=\\"empty\\">Network error</div>";};' +
        'xhr.open("GET","https://transport.opendata.ch/v1/locations?query="+encodeURIComponent(q)+"&type=station");' +
        'xhr.send();}' +

        'document.getElementById("q").addEventListener("keyup",function(e){if(e.keyCode===13)search();});' +

        'function submit(){' +
        'var s=JSON.stringify({' +
        'favorites:favs,' +
        'maxDistance:parseInt(document.getElementById("distance").value)' +
        '});' +
        'document.location.href="pebblejs://close#"+encodeURIComponent(s);}' +

        'renderFavs();' +
        '</script></body></html>';

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

// ── Send stations to watch ───────────────────────────────────

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
    Pebble.sendAppMessage({ 'KEY_STATION': result });
}

// ── Find stations ────────────────────────────────────────────

var s_userLat = 0;
var s_userLon = 0;

function findStations() {
    var settings = loadSettings();
    var maxDist = settings.maxDistance || DEFAULT_MAX_DISTANCE;
    var favorites = settings.favorites || [];

    // Build favorite entries (distance = 0, pinned at top)
    var favEntries = [];
    var favPending = favorites.length;

    if (favPending === 0) {
        // No favorites — just GPS
        findGPSStations(maxDist, function(gpsEntries) {
            sendStationsToWatch(gpsEntries);
        });
        return;
    }

    // Resolve each favorite by ID or name
    var resolved = 0;
    for (var i = 0; i < favorites.length; i++) {
        (function(fav) {
            var url;
            if (fav.id) {
                url = 'http://transport.opendata.ch/v1/locations?query=' +
                      encodeURIComponent(fav.id) + '&type=station&limit=1';
            } else {
                url = 'http://transport.opendata.ch/v1/locations?query=' +
                      encodeURIComponent(fav.name) + '&type=station&limit=1';
            }
            xhrGet(url, function(err, resp) {
                if (!err && resp) {
                    try {
                        var json = JSON.parse(resp);
                        if (json.stations && json.stations.length > 0) {
                            var st = json.stations[0];
                            var parts = splitStationName(st.name);
                            favEntries.push({
                                city: parts.city, stop: parts.stop,
                                id: st.id, distance: 0
                            });
                        }
                    } catch(e) {}
                }
                resolved++;
                if (resolved === favPending) {
                    // All favorites resolved — now get GPS stations
                    findGPSStations(maxDist, function(gpsEntries) {
                        // Merge: favorites first, then GPS (dedup by ID)
                        var merged = favEntries.slice();
                        for (var g = 0; g < gpsEntries.length; g++) {
                            var isDup = false;
                            for (var f = 0; f < merged.length; f++) {
                                if (merged[f].id === gpsEntries[g].id) { isDup = true; break; }
                            }
                            if (!isDup) merged.push(gpsEntries[g]);
                        }
                        sendStationsToWatch(merged);
                    });
                }
            });
        })(favorites[i]);
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
                if (err || !responseText) { callback([]); return; }
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
            callback([]);
        },
        { timeout: 15000, maximumAge: 60000 }
    );
}

// ── Fetch departures ─────────────────────────────────────────

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

Pebble.addEventListener('showConfiguration', function(e) {
    openConfigPage();
});

Pebble.addEventListener('webviewclosed', function(e) {
    if (!e.response) return;
    try {
        var settings = JSON.parse(decodeURIComponent(e.response));
        saveSettings(settings);
        console.log('Bildschirmli: settings saved, ' +
                    (settings.favorites ? settings.favorites.length : 0) + ' favorites');
        // Refresh stations with new settings
        findStations();
    } catch(ex) {
        console.log('Bildschirmli: settings parse error: ' + ex);
    }
});
