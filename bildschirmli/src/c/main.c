/**
 * Bildschirmli — Swiss transit departure board for Pebble.
 *
 * Two-screen flow:
 *   Screen 1 (Station Picker): GPS-based list of nearby stations, sorted by
 *       distance. Amber dot-matrix, selected row inverted. Up/Down to scroll,
 *       Select to pick, Back to exit.
 *   Screen 2 (Departure Board): Departures for selected station. Amber
 *       dot-matrix with LINE | DIRECTION | ETA columns. Up/Down to scroll,
 *       Select or wrist-shake to refresh, Back to return to picker.
 *
 * Platforms: basalt (Pebble Time 144x168), emery (PT2 200x228).
 */

#include <pebble.h>
#include "dm_font.h"
#include "station_ui.h"
#include "transit_ui.h"

// ── AppMessage keys (must match appinfo.json) ────────────────
#define KEY_STATION    0
#define KEY_DEPARTURES 1
#define KEY_ERROR      2
#define KEY_FETCH      3

#define REFRESH_INTERVAL_S 60

// ── App state ────────────────────────────────────────────────

typedef enum {
    STATUS_CONNECTING,       // Waiting for PebbleKit JS ready
    STATUS_LOCATING,         // GPS in progress
    STATUS_LOADING_STATIONS, // Fetching station list
    STATUS_STATIONS_READY,   // Station picker visible
    STATUS_LOADING_DEPS,     // Fetching departures
    STATUS_DEPS_READY,       // Departure board visible
    STATUS_ERROR,            // Error state
} AppStatus;

static AppStatus s_status = STATUS_CONNECTING;
static char s_error_msg[32];

// Station picker data
static StationListData s_stations;

// Departure board data
static TransitData s_transit;
static char s_time_buf[8];

// Windows
static Window *s_picker_window;
static Layer  *s_picker_layer;
static Window *s_deps_window;
static Layer  *s_deps_layer;

static AppTimer *s_refresh_timer;

// ── Status message for current state ─────────────────────────

static const char* status_message(void) {
    switch (s_status) {
        case STATUS_CONNECTING:       return "CONNECTING";
        case STATUS_LOCATING:         return "LOCATING";
        case STATUS_LOADING_STATIONS: return "SEARCHING";
        case STATUS_LOADING_DEPS:     return "LOADING";
        case STATUS_ERROR:            return s_error_msg[0] ? s_error_msg : "ERROR";
        default:                      return "";
    }
}

// ── Time helper ──────────────────────────────────────────────

static void update_time(void) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(s_time_buf, sizeof(s_time_buf),
             clock_is_24h_style() ? "%H:%M" : "%I:%M", t);
}

// ── Station list parser ──────────────────────────────────────
// Format: "city1|stop1:id1:dist1;city2|stop2:id2:dist2;...%"

static void parse_stations(const char *raw) {
    if (!raw || !raw[0]) return;

    memset(&s_stations, 0, sizeof(s_stations));
    s_stations.mode = STATION_DISPLAY_STOP;  // default: Mode A
    const char *p = raw;

    while (*p && *p != '%' && s_stations.count < MAX_STATIONS) {
        StationEntry *st = &s_stations.stations[s_stations.count];

        // City|Stop (separated by pipe)
        const char *colon = strchr(p, ':');
        if (!colon) break;

        // Find the pipe within city|stop
        const char *pipe = p;
        while (pipe < colon && *pipe != '|') pipe++;

        if (pipe < colon && *pipe == '|') {
            // Has city|stop split
            size_t clen = pipe - p;
            if (clen >= sizeof(st->city)) clen = sizeof(st->city) - 1;
            memcpy(st->city, p, clen);
            st->city[clen] = '\0';

            size_t slen = colon - (pipe + 1);
            if (slen >= sizeof(st->stop)) slen = sizeof(st->stop) - 1;
            memcpy(st->stop, pipe + 1, slen);
            st->stop[slen] = '\0';
        } else {
            // No pipe — whole thing is the name, put in city
            size_t len = colon - p;
            if (len >= sizeof(st->city)) len = sizeof(st->city) - 1;
            memcpy(st->city, p, len);
            st->city[len] = '\0';
            st->stop[0] = '\0';
        }
        p = colon + 1;

        // ID
        colon = strchr(p, ':');
        if (!colon) break;
        size_t len = colon - p;
        if (len >= MAX_STATION_ID) len = MAX_STATION_ID - 1;
        memcpy(st->id, p, len);
        st->id[len] = '\0';
        p = colon + 1;

        // Distance
        const char *semi = strchr(p, ';');
        if (!semi) break;
        char dbuf[8];
        len = semi - p;
        if (len >= sizeof(dbuf)) len = sizeof(dbuf) - 1;
        memcpy(dbuf, p, len);
        dbuf[len] = '\0';
        st->distance_m = atoi(dbuf);
        p = semi + 1;

        s_stations.count++;
    }

    // Detect shared city prefix
    s_stations.shared_city = true;
    if (s_stations.count > 1) {
        for (int i = 1; i < s_stations.count; i++) {
            if (strcmp(s_stations.stations[0].city, s_stations.stations[i].city) != 0) {
                s_stations.shared_city = false;
                break;
            }
        }
    }

    s_stations.selected = 0;
    s_stations.scroll_offset = 0;
    s_stations.valid = (s_stations.count > 0);
}

// ── Departure string parser ──────────────────────────────────
// Format: "StationName;line:dir:min;line:dir:min;...%"

static void parse_departures(const char *raw) {
    if (!raw || !raw[0]) return;

    memset(&s_transit, 0, sizeof(s_transit));
    const char *p = raw;

    // Station name
    const char *semi = strchr(p, ';');
    if (!semi) return;
    size_t slen = semi - p;
    if (slen >= MAX_STATION_LEN) slen = MAX_STATION_LEN - 1;
    memcpy(s_transit.station, p, slen);
    s_transit.station[slen] = '\0';
    p = semi + 1;

    // Departures
    while (*p && *p != '%' && s_transit.count < MAX_DEPARTURES) {
        Departure *dep = &s_transit.departures[s_transit.count];

        const char *colon = strchr(p, ':');
        if (!colon) break;
        size_t len = colon - p;
        if (len >= MAX_LINE_LEN) len = MAX_LINE_LEN - 1;
        memcpy(dep->line, p, len);
        dep->line[len] = '\0';
        p = colon + 1;

        colon = strchr(p, ':');
        if (!colon) break;
        len = colon - p;
        if (len >= MAX_DIR_LEN) len = MAX_DIR_LEN - 1;
        memcpy(dep->direction, p, len);
        dep->direction[len] = '\0';
        p = colon + 1;

        semi = strchr(p, ';');
        if (!semi) break;
        len = semi - p;
        if (len >= MAX_ETA_LEN) len = MAX_ETA_LEN - 1;
        memcpy(dep->eta, p, len);
        dep->eta[len] = '\0';
        p = semi + 1;

        s_transit.count++;
    }

    s_transit.scroll_offset = 0;
    s_transit.valid = (s_transit.count > 0);
}

// ── Send messages to phone ───────────────────────────────────

static void request_departures(const char *station_id) {
    DictionaryIterator *out;
    if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
    dict_write_cstring(out, KEY_FETCH, station_id);
    dict_write_end(out);
    app_message_outbox_send();
}

static void request_station_refresh(void) {
    DictionaryIterator *out;
    if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
    dict_write_int8(out, KEY_STATION, 1);
    dict_write_end(out);
    app_message_outbox_send();
}

// ── AppMessage handlers ──────────────────────────────────────

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
    // Station list
    Tuple *station_tuple = dict_find(iter, KEY_STATION);
    if (station_tuple) {
        parse_stations(station_tuple->value->cstring);
        s_status = STATUS_STATIONS_READY;
        if (s_picker_layer) layer_mark_dirty(s_picker_layer);
        return;
    }

    // Departures
    Tuple *dep_tuple = dict_find(iter, KEY_DEPARTURES);
    if (dep_tuple) {
        parse_departures(dep_tuple->value->cstring);
        update_time();
        s_status = STATUS_DEPS_READY;
        if (s_deps_layer) layer_mark_dirty(s_deps_layer);
        return;
    }

    // Error
    Tuple *err_tuple = dict_find(iter, KEY_ERROR);
    if (err_tuple) {
        snprintf(s_error_msg, sizeof(s_error_msg), "%s", err_tuple->value->cstring);
        s_status = STATUS_ERROR;
        // Redraw whichever screen is active
        if (s_deps_layer) layer_mark_dirty(s_deps_layer);
        if (s_picker_layer) layer_mark_dirty(s_picker_layer);
    }
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Inbox dropped: %d", (int)reason);
}

static void outbox_failed_handler(DictionaryIterator *iter,
                                  AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox failed: %d", (int)reason);
}

// ── Departure screen ─────────────────────────────────────────

static void deps_update_proc(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);

    if (s_status == STATUS_DEPS_READY && s_transit.valid) {
        transit_ui_draw(ctx, bounds, &s_transit, s_time_buf);
    } else {
        transit_ui_draw_status(ctx, bounds, status_message());
    }
}

static void deps_select_handler(ClickRecognizerRef ref, void *ctx) {
    // Refresh current station
    if (s_stations.valid && s_stations.selected < s_stations.count) {
        s_status = STATUS_LOADING_DEPS;
        layer_mark_dirty(s_deps_layer);
        request_departures(s_stations.stations[s_stations.selected].id);
    }
}

static void deps_up_handler(ClickRecognizerRef ref, void *ctx) {
    if (s_transit.scroll_offset > 0) {
        s_transit.scroll_offset--;
        layer_mark_dirty(s_deps_layer);
    }
}

static void deps_down_handler(ClickRecognizerRef ref, void *ctx) {
    GRect bounds = layer_get_bounds(window_get_root_layer(s_deps_window));
    int visible = transit_ui_visible_rows(bounds);
    if (s_transit.scroll_offset + visible < s_transit.count) {
        s_transit.scroll_offset++;
        layer_mark_dirty(s_deps_layer);
    }
}

static void deps_back_handler(ClickRecognizerRef ref, void *ctx) {
    // Pop back to station picker
    window_stack_pop(true);
}

static void deps_click_config(void *context) {
    window_single_click_subscribe(BUTTON_ID_SELECT, deps_select_handler);
    window_single_click_subscribe(BUTTON_ID_UP, deps_up_handler);
    window_single_click_subscribe(BUTTON_ID_DOWN, deps_down_handler);
    window_single_click_subscribe(BUTTON_ID_BACK, deps_back_handler);
}

static void deps_tap_handler(AccelAxisType axis, int32_t direction) {
    // Wrist shake = refresh
    deps_select_handler(NULL, NULL);
}

static void refresh_timer_handler(void *data) {
    // Auto-refresh departures if we're on the departure screen
    if (s_status == STATUS_DEPS_READY && s_stations.valid) {
        request_departures(s_stations.stations[s_stations.selected].id);
    }
    s_refresh_timer = app_timer_register(REFRESH_INTERVAL_S * 1000,
                                          refresh_timer_handler, NULL);
}

static void deps_window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(root);

    s_deps_layer = layer_create(bounds);
    layer_set_update_proc(s_deps_layer, deps_update_proc);
    layer_add_child(root, s_deps_layer);

    accel_tap_service_subscribe(deps_tap_handler);

    // Start auto-refresh
    s_refresh_timer = app_timer_register(REFRESH_INTERVAL_S * 1000,
                                          refresh_timer_handler, NULL);
}

static void deps_window_unload(Window *window) {
    accel_tap_service_unsubscribe();
    if (s_refresh_timer) {
        app_timer_cancel(s_refresh_timer);
        s_refresh_timer = NULL;
    }
    layer_destroy(s_deps_layer);
    s_deps_layer = NULL;
}

// ── Tick handler (update clock on departure screen) ──────────

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
    update_time();
    if (s_deps_layer) layer_mark_dirty(s_deps_layer);
}

// ── Station picker screen ────────────────────────────────────

static void picker_update_proc(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);

    if (s_status == STATUS_STATIONS_READY && s_stations.valid) {
        station_ui_draw(ctx, bounds, &s_stations);
    } else {
        transit_ui_draw_status(ctx, bounds, status_message());
    }
}

static void picker_select_handler(ClickRecognizerRef ref, void *ctx) {
    if (!s_stations.valid || s_stations.count == 0) return;

    // Fetch departures for selected station
    const char *id = s_stations.stations[s_stations.selected].id;
    s_status = STATUS_LOADING_DEPS;

    // Only push if not already on the stack (avoid double-push)
    if (window_stack_get_top_window() != s_deps_window) {
        window_stack_push(s_deps_window, true);
    }

    request_departures(id);
}

static void picker_up_handler(ClickRecognizerRef ref, void *ctx) {
    if (!s_stations.valid || s_stations.selected <= 0) return;

    s_stations.selected--;

    // Scroll up if selection moved above visible area
    if (s_stations.selected < s_stations.scroll_offset) {
        s_stations.scroll_offset = s_stations.selected;
    }

    layer_mark_dirty(s_picker_layer);
}

static void picker_down_handler(ClickRecognizerRef ref, void *ctx) {
    if (!s_stations.valid || s_stations.selected >= s_stations.count - 1) return;

    s_stations.selected++;

    // Scroll down if selection moved below visible area
    GRect bounds = layer_get_bounds(window_get_root_layer(s_picker_window));
    int visible = station_ui_visible_rows(bounds);
    if (s_stations.selected >= s_stations.scroll_offset + visible) {
        s_stations.scroll_offset = s_stations.selected - visible + 1;
    }

    layer_mark_dirty(s_picker_layer);
}

static void picker_click_config(void *context) {
    window_single_click_subscribe(BUTTON_ID_SELECT, picker_select_handler);
    window_single_click_subscribe(BUTTON_ID_UP, picker_up_handler);
    window_single_click_subscribe(BUTTON_ID_DOWN, picker_down_handler);
}

static void picker_window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(root);

    s_picker_layer = layer_create(bounds);
    layer_set_update_proc(s_picker_layer, picker_update_proc);
    layer_add_child(root, s_picker_layer);
}

static void picker_window_unload(Window *window) {
    layer_destroy(s_picker_layer);
    s_picker_layer = NULL;
}

// ── App lifecycle ────────────────────────────────────────────

static void init(void) {
    memset(&s_stations, 0, sizeof(s_stations));
    memset(&s_transit, 0, sizeof(s_transit));
    s_status = STATUS_CONNECTING;
    update_time();

    // Picker window (main / root)
    s_picker_window = window_create();
    window_set_background_color(s_picker_window, GColorBlack);
    window_set_click_config_provider(s_picker_window, picker_click_config);
    window_set_window_handlers(s_picker_window, (WindowHandlers) {
        .load = picker_window_load,
        .unload = picker_window_unload,
    });

    // Departure window (pushed on station select)
    s_deps_window = window_create();
    window_set_background_color(s_deps_window, GColorBlack);
    window_set_click_config_provider(s_deps_window, deps_click_config);
    window_set_window_handlers(s_deps_window, (WindowHandlers) {
        .load = deps_window_load,
        .unload = deps_window_unload,
    });

    // Push picker as root
    window_stack_push(s_picker_window, true);

    // AppMessage
    app_message_register_inbox_received(inbox_received_handler);
    app_message_register_inbox_dropped(inbox_dropped_handler);
    app_message_register_outbox_failed(outbox_failed_handler);
    app_message_open(2048, 256);

    // Tick timer for clock on departure screen
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void deinit(void) {
    tick_timer_service_unsubscribe();
    window_destroy(s_deps_window);
    window_destroy(s_picker_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
