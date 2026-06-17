#include "station_ui.h"
#include "dm_font.h"
#include <stdio.h>
#include <string.h>

int station_ui_visible_rows(GRect bounds) {
    int header_h = dm_text_height(DM_HEADER_DOT) + DM_ROW_GAP + 4;
    int row_h = dm_text_height(DM_ROW_DOT) + DM_ROW_GAP + 4;
    return (bounds.size.h - DM_MARGIN_Y - header_h) / row_h;
}

// Format a station name based on display mode
static void format_station_name(const StationListData *data, int idx,
                                char *out, size_t out_len) {
    const StationEntry *st = &data->stations[idx];

    if (data->mode == STATION_DISPLAY_STOP || data->shared_city) {
        // Mode A: just the stop part
        if (st->stop[0]) {
            snprintf(out, out_len, "%s", st->stop);
        } else {
            // No comma in original name — use city as fallback
            snprintf(out, out_len, "%s", st->city);
        }
    } else {
        // Mode B: "CIT STOP" — 3-char city abbreviation + stop
        if (st->stop[0] && st->city[0]) {
            char city_abbr[4];
            size_t clen = strlen(st->city);
            if (clen > 3) clen = 3;
            memcpy(city_abbr, st->city, clen);
            city_abbr[clen] = '\0';
            snprintf(out, out_len, "%s %s", city_abbr, st->stop);
        } else if (st->stop[0]) {
            snprintf(out, out_len, "%s", st->stop);
        } else {
            snprintf(out, out_len, "%s", st->city);
        }
    }
}

void station_ui_draw(GContext *ctx, GRect bounds, const StationListData *data) {
    GColor amber = DM_COLOR_CURRENT();
    GColor black = DM_BG_CURRENT();
    int w = bounds.size.w;

    // Black background
    graphics_context_set_fill_color(ctx, black);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    int content_w = w - 2 * DM_MARGIN_X;
    int y = DM_MARGIN_Y;

    // Header
    dm_text(ctx, DM_MARGIN_X, y, "STATIONS", content_w, DM_HEADER_DOT, amber);
    y += dm_text_height(DM_HEADER_DOT) + DM_ROW_GAP;

    // Separator
    graphics_context_set_stroke_color(ctx, amber);
    graphics_draw_line(ctx, GPoint(DM_MARGIN_X, y), GPoint(w - DM_MARGIN_X - 1, y));
    y += DM_ROW_GAP + 2;

    if (!data || !data->valid || data->count == 0) {
        dm_text(ctx, DM_MARGIN_X, y, "LOCATING...", content_w, DM_ROW_DOT, amber);
        return;
    }

    int row_text_h = dm_text_height(DM_ROW_DOT);
    int row_h = row_text_h + DM_ROW_GAP + 4;
    int visible = station_ui_visible_rows(bounds);

    // Distance column: 3 chars ("459", "800"), no unit — always meters at <1km
    int dist_col_w = 3 * dm_char_width(DM_ROW_DOT) + 2;
    int name_col_w = content_w - dist_col_w - 4;

    for (int vi = 0; vi < visible && (data->scroll_offset + vi) < data->count; vi++) {
        int idx = data->scroll_offset + vi;
        const StationEntry *st = &data->stations[idx];
        bool selected = (idx == data->selected);

        int row_y = y + vi * row_h;

        if (selected) {
            // Inverted: amber background, black text
            graphics_context_set_fill_color(ctx, amber);
            graphics_fill_rect(ctx,
                GRect(DM_MARGIN_X - 2, row_y - 2, content_w + 4, row_text_h + 4),
                0, GCornerNone);
        }

        GColor text_color = selected ? black : amber;

        // Station name — formatted based on display mode
        char display_name[MAX_STATION_NAME];
        format_station_name(data, idx, display_name, sizeof(display_name));
        char san[MAX_STATION_NAME];
        dm_sanitize(display_name, san, sizeof(san));
        dm_text(ctx, DM_MARGIN_X, row_y, san, name_col_w, DM_ROW_DOT, text_color);

        // Distance — plain number, no unit
        char dist[8];
        snprintf(dist, sizeof(dist), "%d", st->distance_m);
        char sdist[8];
        dm_sanitize(dist, sdist, sizeof(sdist));
        int dw = dm_text_width(sdist, DM_ROW_DOT);
        dm_text(ctx, w - DM_MARGIN_X - dw, row_y, sdist, dist_col_w,
                DM_ROW_DOT, text_color);
    }

    // Scroll indicators
    if (data->scroll_offset > 0) {
        int ax = w - DM_MARGIN_X - 6;
        int ay = DM_MARGIN_Y + dm_text_height(DM_HEADER_DOT) + 2;
        graphics_context_set_fill_color(ctx, amber);
        graphics_fill_rect(ctx, GRect(ax + 1, ay, 4, 1), 0, GCornerNone);
        graphics_fill_rect(ctx, GRect(ax + 2, ay - 1, 2, 1), 0, GCornerNone);
    }
    if (data->scroll_offset + visible < data->count) {
        int ax = w - DM_MARGIN_X - 6;
        int ay = bounds.size.h - DM_MARGIN_Y;
        graphics_context_set_fill_color(ctx, amber);
        graphics_fill_rect(ctx, GRect(ax + 1, ay - 2, 4, 1), 0, GCornerNone);
        graphics_fill_rect(ctx, GRect(ax + 2, ay - 1, 2, 1), 0, GCornerNone);
    }
}
