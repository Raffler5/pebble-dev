#pragma once

#include <pebble.h>

#define MAX_DEPARTURES 8
#define MAX_LINE_LEN   8
#define MAX_DIR_LEN    32
#define MAX_ETA_LEN    8
#define MAX_STATION_LEN 40

typedef struct {
    char line[MAX_LINE_LEN];
    char direction[MAX_DIR_LEN];
    char eta[MAX_ETA_LEN];         // relative minutes as string, or "NOW"
} Departure;

typedef struct {
    char station[MAX_STATION_LEN];
    Departure departures[MAX_DEPARTURES];
    int count;
    bool valid;
} TransitData;

// Draw the full transit page into the given layer's graphics context.
// Call from the layer's update_proc.
void transit_ui_draw(GContext *ctx, GRect bounds, const TransitData *data);

// Draw a loading/error message centered on screen
void transit_ui_draw_status(GContext *ctx, GRect bounds, const char *message);
