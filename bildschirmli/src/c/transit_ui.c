#include "transit_ui.h"
#include "dm_font.h"
#include <string.h>

void transit_ui_draw(GContext *ctx, GRect bounds, const TransitData *data) {
    GColor amber = (GColor){ .argb = DM_COLOR_AMBER };
    GColor bg = GColorBlack;
    int w = bounds.size.w;
    int h = bounds.size.h;

    // Black background
    graphics_context_set_fill_color(ctx, bg);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    if (!data || !data->valid || data->count == 0) {
        transit_ui_draw_status(ctx, bounds, data ? "NO DATA" : "LOADING");
        return;
    }

    int content_w = w - 2 * DM_MARGIN_X;
    int y = DM_MARGIN_Y;

    // Station name — header dot size
    char san[MAX_STATION_LEN];
    dm_sanitize(data->station, san, sizeof(san));
    dm_text(ctx, DM_MARGIN_X, y, san, content_w, DM_HEADER_DOT, amber);
    y += dm_text_height(DM_HEADER_DOT) + DM_ROW_GAP;

    // Thin amber separator
    graphics_context_set_stroke_color(ctx, amber);
    graphics_draw_line(ctx,
        GPoint(DM_MARGIN_X, y),
        GPoint(w - DM_MARGIN_X - 1, y));
    y += DM_ROW_GAP + 2;

    // Departure rows — smaller dot size
    int row_char_w = dm_char_width(DM_ROW_DOT);
    int row_text_h = dm_text_height(DM_ROW_DOT);
    int row_h = row_text_h + DM_ROW_GAP + 2;

    // Column widths
    int line_col_w = 4 * row_char_w;          // "S25" fits in 3-4 chars
    int eta_col_w  = 4 * row_char_w;          // "120'" or bus icon
    int dir_col_w  = content_w - line_col_w - eta_col_w - 4;

    int max_rows = (h - y) / row_h;
    int count = data->count < max_rows ? data->count : max_rows;

    for (int i = 0; i < count; i++) {
        const Departure *dep = &data->departures[i];

        // Line number
        char sline[MAX_LINE_LEN];
        dm_sanitize(dep->line, sline, sizeof(sline));
        dm_text(ctx, DM_MARGIN_X, y, sline, line_col_w, DM_ROW_DOT, amber);

        // Direction
        char sdir[MAX_DIR_LEN];
        dm_sanitize(dep->direction, sdir, sizeof(sdir));
        dm_text(ctx, DM_MARGIN_X + line_col_w + 2, y, sdir, dir_col_w, DM_ROW_DOT, amber);

        // ETA — bus icon for "NOW", otherwise right-aligned minutes
        if (strcmp(dep->eta, "0") == 0 || strcasecmp(dep->eta, "NOW") == 0) {
            int pitch = DM_ROW_DOT + 1;
            int icon_w = 8 * pitch;
            dm_bus_icon(ctx, w - DM_MARGIN_X - icon_w, y - 1, DM_ROW_DOT, amber);
        } else {
            char seta[MAX_ETA_LEN];
            dm_sanitize(dep->eta, seta, sizeof(seta));
            // Append minute mark
            size_t len = strlen(seta);
            if (len < MAX_ETA_LEN - 2) {
                seta[len] = '\'';
                seta[len + 1] = '\0';
            }
            int eta_w = dm_text_width(seta, DM_ROW_DOT);
            dm_text(ctx, w - DM_MARGIN_X - eta_w, y, seta, eta_col_w, DM_ROW_DOT, amber);
        }

        y += row_h;
    }
}

void transit_ui_draw_status(GContext *ctx, GRect bounds, const char *message) {
    GColor amber = (GColor){ .argb = DM_COLOR_AMBER };
    GColor bg = GColorBlack;

    graphics_context_set_fill_color(ctx, bg);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    char san[32];
    dm_sanitize(message, san, sizeof(san));
    int tw = dm_text_width(san, DM_HEADER_DOT);
    int th = dm_text_height(DM_HEADER_DOT);
    int x = (bounds.size.w - tw) / 2;
    int y = (bounds.size.h - th) / 2;
    dm_text(ctx, x, y, san, bounds.size.w, DM_HEADER_DOT, amber);
}
