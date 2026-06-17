#pragma once

#include <pebble.h>

#define MAX_DEPARTURES  12
#define MAX_LINE_LEN     8
#define MAX_DIR_LEN     32
#define MAX_ETA_LEN      8
#define MAX_STATION_LEN 40
#define MAX_PLATFORM_LEN 8

typedef struct {
    char line[MAX_LINE_LEN];
    char direction[MAX_DIR_LEN];
    char eta[MAX_ETA_LEN];         // relative minutes as string, or "0" for NOW
    char platform[MAX_PLATFORM_LEN]; // track/platform, empty if none
} Departure;

typedef struct {
    char station[MAX_STATION_LEN];
    Departure departures[MAX_DEPARTURES];
    int count;
    int scroll_offset;             // first visible departure row
    int selected;                  // currently highlighted departure
    bool selecting;                // true once user activates cursor
    bool valid;
} TransitData;

// Draw the full transit departure board.
// time_str should be "HH:MM" or NULL to omit.
void transit_ui_draw(GContext *ctx, GRect bounds, const TransitData *data,
                     const char *time_str);

// Draw a centered status message (loading / error)
void transit_ui_draw_status(GContext *ctx, GRect bounds, const char *message);

// How many departure rows fit on screen
int transit_ui_visible_rows(GRect bounds);

// Route stops for detail view (loaded on demand)
#define MAX_ROUTE_STOPS 24
#define MAX_STOP_NAME   24

typedef struct {
    char stops[MAX_ROUTE_STOPS][MAX_STOP_NAME];
    int count;
    int scroll_offset;
    bool valid;
    bool loading;
} RouteData;

// Draw the detail view for a single departure
void transit_ui_draw_detail(GContext *ctx, GRect bounds, const TransitData *data,
                            int dep_index, const RouteData *route);
