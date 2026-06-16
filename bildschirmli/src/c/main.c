/**
 * Bildschirmli — Swiss transit departure board for Pebble.
 *
 * Follows the official SDK pattern from pebble-sdk-examples:
 *   - ALL windows created at init, destroyed at deinit
 *   - Layers created in .load, destroyed in .unload
 *   - .appear used for state refresh (not just mark_dirty)
 *   - Back button: SDK default (no subscription = pop)
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

// ── App state (file-scope statics, the Pebble way) ──────────

typedef enum {
    STATUS_CONNECTING,
    STATUS_LOCATING,
    STATUS_LOADING_STATIONS,
    STATUS_STATIONS_READY,
    STATUS_LOADING_DEPS,
    STATUS_DEPS_READY,
    STATUS_ERROR,
} AppStatus;

static AppStatus s_status = STATUS_CONNECTING;
static char s_error_msg[32];
static StationListData s_stations;
static TransitData s_transit;
static char s_time_buf[8];

// Both windows created at init, destroyed at deinit
static Window *s_picker_window;
static Window *s_deps_window;

// Layers created in .load, destroyed in .unload
static Layer *s_picker_layer;
static Layer *s_deps_layer;

static AppTimer *s_refresh_timer;

// ── Helpers ──────────────────────────────────────────────────

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

static void update_time(void) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(s_time_buf, sizeof(s_time_buf),
             clock_is_24h_style() ? "%H:%M" : "%I:%M", t);
}

// ── Parsers ──────────────────────────────────────────────────

static void parse_stations(const char *raw) {
    if (!raw || !raw[0]) return;
    memset(&s_stations, 0, sizeof(s_stations));
    s_stations.mode = STATION_DISPLAY_STOP;
    const char *p = raw;

    while (*p && *p != '%' && s_stations.count < MAX_STATIONS) {
        StationEntry *st = &s_stations.stations[s_stations.count];

        const char *colon = strchr(p, ':');
        if (!colon) break;
        const char *pipe = p;
        while (pipe < colon && *pipe != '|') pipe++;

        if (pipe < colon && *pipe == '|') {
            size_t clen = pipe - p;
            if (clen >= sizeof(st->city)) clen = sizeof(st->city) - 1;
            memcpy(st->city, p, clen); st->city[clen] = '\0';
            size_t slen = colon - (pipe + 1);
            if (slen >= sizeof(st->stop)) slen = sizeof(st->stop) - 1;
            memcpy(st->stop, pipe + 1, slen); st->stop[slen] = '\0';
        } else {
            size_t len = colon - p;
            if (len >= sizeof(st->city)) len = sizeof(st->city) - 1;
            memcpy(st->city, p, len); st->city[len] = '\0';
            st->stop[0] = '\0';
        }
        p = colon + 1;

        colon = strchr(p, ':');
        if (!colon) break;
        size_t len = colon - p;
        if (len >= MAX_STATION_ID) len = MAX_STATION_ID - 1;
        memcpy(st->id, p, len); st->id[len] = '\0';
        p = colon + 1;

        const char *semi = strchr(p, ';');
        if (!semi) break;
        char dbuf[8];
        len = semi - p;
        if (len >= sizeof(dbuf)) len = sizeof(dbuf) - 1;
        memcpy(dbuf, p, len); dbuf[len] = '\0';
        st->distance_m = atoi(dbuf);
        p = semi + 1;

        s_stations.count++;
    }

    s_stations.shared_city = true;
    for (int i = 1; i < s_stations.count; i++) {
        if (strcmp(s_stations.stations[0].city, s_stations.stations[i].city) != 0) {
            s_stations.shared_city = false; break;
        }
    }
    s_stations.selected = 0;
    s_stations.scroll_offset = 0;
    s_stations.valid = (s_stations.count > 0);
}

static void parse_departures(const char *raw) {
    if (!raw || !raw[0]) return;
    memset(&s_transit, 0, sizeof(s_transit));
    const char *p = raw;

    const char *semi = strchr(p, ';');
    if (!semi) return;
    size_t slen = semi - p;
    if (slen >= MAX_STATION_LEN) slen = MAX_STATION_LEN - 1;
    memcpy(s_transit.station, p, slen); s_transit.station[slen] = '\0';
    p = semi + 1;

    while (*p && *p != '%' && s_transit.count < MAX_DEPARTURES) {
        Departure *dep = &s_transit.departures[s_transit.count];
        const char *colon = strchr(p, ':');
        if (!colon) break;
        size_t len = colon - p;
        if (len >= MAX_LINE_LEN) len = MAX_LINE_LEN - 1;
        memcpy(dep->line, p, len); dep->line[len] = '\0';
        p = colon + 1;

        colon = strchr(p, ':');
        if (!colon) break;
        len = colon - p;
        if (len >= MAX_DIR_LEN) len = MAX_DIR_LEN - 1;
        memcpy(dep->direction, p, len); dep->direction[len] = '\0';
        p = colon + 1;

        semi = strchr(p, ';');
        if (!semi) break;
        len = semi - p;
        if (len >= MAX_ETA_LEN) len = MAX_ETA_LEN - 1;
        memcpy(dep->eta, p, len); dep->eta[len] = '\0';
        p = semi + 1;

        s_transit.count++;
    }
    s_transit.scroll_offset = 0;
    s_transit.valid = (s_transit.count > 0);
}

// ── Phone communication ──────────────────────────────────────

static void request_departures(const char *station_id) {
    DictionaryIterator *out;
    if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
    dict_write_cstring(out, KEY_FETCH, station_id);
    dict_write_end(out);
    app_message_outbox_send();
}

// ── AppMessage handlers ──────────────────────────────────────

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
    Tuple *t;

    t = dict_find(iter, KEY_STATION);
    if (t) {
        parse_stations(t->value->cstring);
        s_status = STATUS_STATIONS_READY;
        if (s_picker_layer) layer_mark_dirty(s_picker_layer);
        return;
    }

    t = dict_find(iter, KEY_DEPARTURES);
    if (t) {
        parse_departures(t->value->cstring);
        update_time();
        s_status = STATUS_DEPS_READY;
        if (s_deps_layer) layer_mark_dirty(s_deps_layer);
        return;
    }

    t = dict_find(iter, KEY_ERROR);
    if (t) {
        snprintf(s_error_msg, sizeof(s_error_msg), "%s", t->value->cstring);
        s_status = STATUS_ERROR;
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
    if (s_stations.valid && s_stations.selected < s_stations.count) {
        s_status = STATUS_LOADING_DEPS;
        if (s_deps_layer) layer_mark_dirty(s_deps_layer);
        request_departures(s_stations.stations[s_stations.selected].id);
    }
}

static void deps_up_handler(ClickRecognizerRef ref, void *ctx) {
    if (s_transit.scroll_offset > 0) {
        s_transit.scroll_offset--;
        if (s_deps_layer) layer_mark_dirty(s_deps_layer);
    }
}

static void deps_down_handler(ClickRecognizerRef ref, void *ctx) {
    GRect bounds = layer_get_bounds(window_get_root_layer(s_deps_window));
    int visible = transit_ui_visible_rows(bounds);
    if (s_transit.scroll_offset + visible < s_transit.count) {
        s_transit.scroll_offset++;
        if (s_deps_layer) layer_mark_dirty(s_deps_layer);
    }
}

static void deps_tap_handler(AccelAxisType axis, int32_t direction) {
    deps_select_handler(NULL, NULL);
}

static void refresh_timer_handler(void *data) {
    if (s_status == STATUS_DEPS_READY && s_stations.valid) {
        request_departures(s_stations.stations[s_stations.selected].id);
    }
    s_refresh_timer = app_timer_register(REFRESH_INTERVAL_S * 1000,
                                          refresh_timer_handler, NULL);
}

static void deps_click_config(void *context) {
    window_single_click_subscribe(BUTTON_ID_SELECT, deps_select_handler);
    window_single_click_subscribe(BUTTON_ID_UP, deps_up_handler);
    window_single_click_subscribe(BUTTON_ID_DOWN, deps_down_handler);
    // BACK not subscribed → SDK default pops window
}

static void deps_window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(root);
    s_deps_layer = layer_create(bounds);
    layer_set_update_proc(s_deps_layer, deps_update_proc);
    layer_add_child(root, s_deps_layer);

    accel_tap_service_subscribe(deps_tap_handler);
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
    // Window is NOT destroyed here — destroyed in deinit()
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
    s_status = STATUS_LOADING_DEPS;
    window_stack_push(s_deps_window, true);
    request_departures(s_stations.stations[s_stations.selected].id);
}

static void picker_up_handler(ClickRecognizerRef ref, void *ctx) {
    if (!s_stations.valid || s_stations.selected <= 0) return;
    s_stations.selected--;
    if (s_stations.selected < s_stations.scroll_offset)
        s_stations.scroll_offset = s_stations.selected;
    layer_mark_dirty(s_picker_layer);
}

static void picker_down_handler(ClickRecognizerRef ref, void *ctx) {
    if (!s_stations.valid || s_stations.selected >= s_stations.count - 1) return;
    s_stations.selected++;
    GRect bounds = layer_get_bounds(window_get_root_layer(s_picker_window));
    int visible = station_ui_visible_rows(bounds);
    if (s_stations.selected >= s_stations.scroll_offset + visible)
        s_stations.scroll_offset = s_stations.selected - visible + 1;
    layer_mark_dirty(s_picker_layer);
}

static void picker_click_config(void *context) {
    window_single_click_subscribe(BUTTON_ID_SELECT, picker_select_handler);
    window_single_click_subscribe(BUTTON_ID_UP, picker_up_handler);
    window_single_click_subscribe(BUTTON_ID_DOWN, picker_down_handler);
    // BACK not subscribed → SDK default exits app
}

static void picker_window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(root);
    s_picker_layer = layer_create(bounds);
    layer_set_update_proc(s_picker_layer, picker_update_proc);
    layer_add_child(root, s_picker_layer);
}

// .appear — fires every time picker becomes visible (including after back)
// Following pebble_arcade pattern: reset relevant state, then data is ready
// for the next update_proc call which the SDK triggers after .appear
static void picker_window_appear(Window *window) {
    s_status = STATUS_STATIONS_READY;
    if (s_picker_layer) layer_mark_dirty(s_picker_layer);
}

static void picker_window_unload(Window *window) {
    layer_destroy(s_picker_layer);
    s_picker_layer = NULL;
}

// ── Tick handler ─────────────────────────────────────────────

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
    update_time();
    if (s_deps_layer) layer_mark_dirty(s_deps_layer);
}

// ── App lifecycle ────────────────────────────────────────────

static void init(void) {
    memset(&s_stations, 0, sizeof(s_stations));
    memset(&s_transit, 0, sizeof(s_transit));
    s_status = STATUS_CONNECTING;
    update_time();

    // Create BOTH windows at init (official SDK pattern)
    s_picker_window = window_create();
    window_set_background_color(s_picker_window, GColorBlack);
    window_set_click_config_provider(s_picker_window, picker_click_config);
    window_set_window_handlers(s_picker_window, (WindowHandlers) {
        .load = picker_window_load,
        .appear = picker_window_appear,
        .unload = picker_window_unload,
    });

    s_deps_window = window_create();
    window_set_background_color(s_deps_window, GColorBlack);
    window_set_click_config_provider(s_deps_window, deps_click_config);
    window_set_window_handlers(s_deps_window, (WindowHandlers) {
        .load = deps_window_load,
        .unload = deps_window_unload,
    });

    // Only push the picker (root window)
    window_stack_push(s_picker_window, true);

    // AppMessage
    app_message_register_inbox_received(inbox_received_handler);
    app_message_register_inbox_dropped(inbox_dropped_handler);
    app_message_register_outbox_failed(outbox_failed_handler);
    app_message_open(2048, 256);

    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void deinit(void) {
    tick_timer_service_unsubscribe();
    // Destroy BOTH windows at deinit (official SDK pattern)
    window_destroy(s_deps_window);
    window_destroy(s_picker_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
