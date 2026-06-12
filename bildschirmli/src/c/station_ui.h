#pragma once

#include <pebble.h>

#define MAX_STATIONS     10
#define MAX_STATION_NAME 36
#define MAX_STATION_ID   12

typedef struct {
    char name[MAX_STATION_NAME];
    char id[MAX_STATION_ID];
    int  distance_m;             // distance in meters from GPS position
} StationEntry;

typedef struct {
    StationEntry stations[MAX_STATIONS];
    int count;
    int selected;                // currently highlighted index
    int scroll_offset;           // first visible row index
    bool valid;
} StationListData;

// Draw the station picker into the given graphics context.
// Selected row is inverted (black text on amber bg).
void station_ui_draw(GContext *ctx, GRect bounds, const StationListData *data);

// How many rows fit on screen (depends on platform)
int station_ui_visible_rows(GRect bounds);
