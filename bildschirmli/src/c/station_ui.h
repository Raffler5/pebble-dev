#pragma once

#include <pebble.h>

#define MAX_STATIONS     6
#define MAX_STATION_NAME 36
#define MAX_STATION_ID   12

// Display mode for station names
typedef enum {
    STATION_DISPLAY_STOP,    // Mode A: show stop part only ("BAHNHOF", "ROSENWIES.")
    STATION_DISPLAY_CITY,    // Mode B: abbreviated city + stop ("LUF BHF", "EMB BHF")
} StationDisplayMode;

typedef struct {
    char city[16];               // city part (before comma)
    char stop[24];               // stop part (after comma), abbreviated
    char id[MAX_STATION_ID];
    int  distance_m;
} StationEntry;

typedef struct {
    StationEntry stations[MAX_STATIONS];
    int count;
    int selected;
    int scroll_offset;
    bool valid;
    bool shared_city;            // true if all stations share the same city prefix
    StationDisplayMode mode;
} StationListData;

// Draw the station picker into the given graphics context.
// Selected row is inverted (black text on amber bg).
void station_ui_draw(GContext *ctx, GRect bounds, const StationListData *data);

// How many rows fit on screen (depends on platform)
int station_ui_visible_rows(GRect bounds);
