/**
 * Bildschirmli — Swiss transit departure board for Pebble.
 *
 * Amber dot-matrix display style matching the SmallTV-Ultra hardware version.
 * GPS-based nearest station, auto-refresh every 60s.
 *
 * Platforms: basalt (Pebble Time 144x168), emery (Pebble Time 2 200x228).
 */

#include <pebble.h>
#include "transit_ui.h"

#define KEY_STATION    0
#define KEY_DEPARTURES 1
#define KEY_ERROR      2
#define KEY_FETCH      3

#define REFRESH_INTERVAL_S 60

static Window *s_window;
static Layer *s_canvas_layer;
static TransitData s_transit;
static char s_status_msg[32];
static bool s_loading = true;

// ── Departure string parser ──────────────────────────────────
// Format from PKJS: "StationName;line:dir:min;line:dir:min;...%"
static void parse_departures(const char *raw) {
    if (!raw || !raw[0]) return;

    memset(&s_transit, 0, sizeof(s_transit));

    const char *p = raw;

    // Station name (up to first ';')
    const char *semi = strchr(p, ';');
    if (!semi) return;
    size_t slen = semi - p;
    if (slen >= MAX_STATION_LEN) slen = MAX_STATION_LEN - 1;
    memcpy(s_transit.station, p, slen);
    s_transit.station[slen] = '\0';
    p = semi + 1;

    // Departure rows: "line:dir:min;"
    while (*p && *p != '%' && s_transit.count < MAX_DEPARTURES) {
        Departure *dep = &s_transit.departures[s_transit.count];

        // Line
        const char *colon = strchr(p, ':');
        if (!colon) break;
        size_t len = colon - p;
        if (len >= MAX_LINE_LEN) len = MAX_LINE_LEN - 1;
        memcpy(dep->line, p, len);
        dep->line[len] = '\0';
        p = colon + 1;

        // Direction
        colon = strchr(p, ':');
        if (!colon) break;
        len = colon - p;
        if (len >= MAX_DIR_LEN) len = MAX_DIR_LEN - 1;
        memcpy(dep->direction, p, len);
        dep->direction[len] = '\0';
        p = colon + 1;

        // Minutes
        semi = strchr(p, ';');
        if (!semi) break;
        len = semi - p;
        if (len >= MAX_ETA_LEN) len = MAX_ETA_LEN - 1;
        memcpy(dep->eta, p, len);
        dep->eta[len] = '\0';
        p = semi + 1;

        s_transit.count++;
    }

    s_transit.valid = (s_transit.count > 0);
    s_loading = false;
}

// ── Canvas update ────────────────────────────────────────────
static void canvas_update_proc(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);

    if (s_loading) {
        transit_ui_draw_status(ctx, bounds, s_status_msg[0] ? s_status_msg : "LOADING");
    } else if (s_transit.valid) {
        transit_ui_draw(ctx, bounds, &s_transit);
    } else {
        transit_ui_draw_status(ctx, bounds, s_status_msg[0] ? s_status_msg : "NO DATA");
    }
}

// ── AppMessage ───────────────────────────────────────────────
static void inbox_received_handler(DictionaryIterator *iter, void *context) {
    Tuple *dep_tuple = dict_find(iter, KEY_DEPARTURES);
    if (dep_tuple) {
        parse_departures(dep_tuple->value->cstring);
        layer_mark_dirty(s_canvas_layer);
        return;
    }

    Tuple *err_tuple = dict_find(iter, KEY_ERROR);
    if (err_tuple) {
        snprintf(s_status_msg, sizeof(s_status_msg), "%s", err_tuple->value->cstring);
        s_loading = false;
        s_transit.valid = false;
        layer_mark_dirty(s_canvas_layer);
    }
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", (int)reason);
}

static void outbox_failed_handler(DictionaryIterator *iter, AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox failed: %d", (int)reason);
}

// ── Request refresh from phone ───────────────────────────────
static void request_refresh(void) {
    DictionaryIterator *out;
    AppMessageResult result = app_message_outbox_begin(&out);
    if (result != APP_MSG_OK) return;

    dict_write_int8(out, KEY_FETCH, 1);
    dict_write_end(out);
    app_message_outbox_send();
}

// ── Timer: auto-refresh ──────────────────────────────────────
static void refresh_timer_handler(void *data) {
    request_refresh();
    app_timer_register(REFRESH_INTERVAL_S * 1000, refresh_timer_handler, NULL);
}

// ── Button: select = manual refresh ──────────────────────────
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
    snprintf(s_status_msg, sizeof(s_status_msg), "REFRESHING");
    s_loading = true;
    layer_mark_dirty(s_canvas_layer);
    request_refresh();
}

static void click_config_provider(void *context) {
    window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

// ── Window lifecycle ─────────────────────────────────────────
static void window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(root);

    s_canvas_layer = layer_create(bounds);
    layer_set_update_proc(s_canvas_layer, canvas_update_proc);
    layer_add_child(root, s_canvas_layer);
}

static void window_unload(Window *window) {
    layer_destroy(s_canvas_layer);
}

// ── App lifecycle ────────────────────────────────────────────
static void init(void) {
    // Init state
    memset(&s_transit, 0, sizeof(s_transit));
    snprintf(s_status_msg, sizeof(s_status_msg), "CONNECTING");
    s_loading = true;

    // Window
    s_window = window_create();
    window_set_background_color(s_window, GColorBlack);
    window_set_click_config_provider(s_window, click_config_provider);
    window_set_window_handlers(s_window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,
    });
    window_stack_push(s_window, true);

    // AppMessage
    app_message_register_inbox_received(inbox_received_handler);
    app_message_register_inbox_dropped(inbox_dropped_handler);
    app_message_register_outbox_failed(outbox_failed_handler);
    app_message_open(1024, 128);

    // Auto-refresh timer
    app_timer_register(REFRESH_INTERVAL_S * 1000, refresh_timer_handler, NULL);
}

static void deinit(void) {
    window_destroy(s_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
