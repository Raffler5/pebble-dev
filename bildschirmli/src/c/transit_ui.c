#include "transit_ui.h"
#include "dm_font.h"
#include <string.h>

static int header_height(void) {
    return dm_text_height(DM_HEADER_DOT) + DM_ROW_GAP + 4;
}

static int row_height(void) {
    return dm_text_height(DM_ROW_DOT) + DM_ROW_GAP + 2;
}

int transit_ui_visible_rows(GRect bounds) {
    return (bounds.size.h - DM_MARGIN_Y - header_height()) / row_height();
}

void transit_ui_draw(GContext *ctx, GRect bounds, const TransitData *data,
                     const char *time_str) {
    GColor amber = (GColor){ .argb = DM_COLOR_AMBER };
    GColor black = GColorBlack;
    int w = bounds.size.w;

    // Black background
    graphics_context_set_fill_color(ctx, black);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    if (!data || !data->valid || data->count == 0) {
        transit_ui_draw_status(ctx, bounds, data ? "NO DATA" : "LOADING");
        return;
    }

    int content_w = w - 2 * DM_MARGIN_X;
    int y = DM_MARGIN_Y;

    // Header row: station name (left) + time (right)
    char san[MAX_STATION_LEN];
    dm_sanitize(data->station, san, sizeof(san));

    if (time_str && time_str[0]) {
        // Time on the right
        char stime[8];
        dm_sanitize(time_str, stime, sizeof(stime));
        int time_w = dm_text_width(stime, DM_HEADER_DOT);
        dm_text(ctx, w - DM_MARGIN_X - time_w, y, stime, time_w + 2,
                DM_HEADER_DOT, amber);
        // Station name on the left, truncated to leave room for time
        int name_max = content_w - time_w - dm_char_width(DM_HEADER_DOT) * 2;
        dm_text(ctx, DM_MARGIN_X, y, san, name_max, DM_HEADER_DOT, amber);
    } else {
        dm_text(ctx, DM_MARGIN_X, y, san, content_w, DM_HEADER_DOT, amber);
    }

    y += dm_text_height(DM_HEADER_DOT) + DM_ROW_GAP;

    // Separator
    graphics_context_set_stroke_color(ctx, amber);
    graphics_draw_line(ctx, GPoint(DM_MARGIN_X, y), GPoint(w - DM_MARGIN_X - 1, y));
    y += DM_ROW_GAP + 2;

    // Departure rows
    int row_char_w = dm_char_width(DM_ROW_DOT);
    int rh = row_height();

    // Column widths
    int line_col_w = 4 * row_char_w;
    int eta_col_w  = 5 * row_char_w;
    int dir_col_w  = content_w - line_col_w - eta_col_w - 4;

    int visible = transit_ui_visible_rows(bounds);
    int offset = data->scroll_offset;
    int count = data->count - offset;
    if (count > visible) count = visible;

    for (int i = 0; i < count; i++) {
        const Departure *dep = &data->departures[offset + i];

        // Line number
        char sline[MAX_LINE_LEN];
        dm_sanitize(dep->line, sline, sizeof(sline));
        dm_text(ctx, DM_MARGIN_X, y, sline, line_col_w, DM_ROW_DOT, amber);

        // Direction
        char sdir[MAX_DIR_LEN];
        dm_sanitize(dep->direction, sdir, sizeof(sdir));
        dm_text(ctx, DM_MARGIN_X + line_col_w + 2, y, sdir, dir_col_w,
                DM_ROW_DOT, amber);

        // ETA
        if (strcmp(dep->eta, "0") == 0) {
            // Bus icon for "departing now"
            int pitch = DM_ROW_DOT + 1;
            int icon_w = 8 * pitch;
            dm_bus_icon(ctx, w - DM_MARGIN_X - icon_w, y - 1, DM_ROW_DOT, amber);
        } else {
            char seta[MAX_ETA_LEN + 2];
            dm_sanitize(dep->eta, seta, sizeof(seta) - 2);
            // Append minute mark
            size_t len = strlen(seta);
            if (len < sizeof(seta) - 2) {
                seta[len] = '\'';
                seta[len + 1] = '\0';
            }
            int eta_w = dm_text_width(seta, DM_ROW_DOT);
            dm_text(ctx, w - DM_MARGIN_X - eta_w, y, seta, eta_col_w,
                    DM_ROW_DOT, amber);
        }

        y += rh;
    }

    // Scroll indicators
    if (offset > 0) {
        int ax = w - DM_MARGIN_X - 6;
        int ay = DM_MARGIN_Y + dm_text_height(DM_HEADER_DOT) + 2;
        graphics_context_set_fill_color(ctx, amber);
        graphics_fill_rect(ctx, GRect(ax + 1, ay, 4, 1), 0, GCornerNone);
        graphics_fill_rect(ctx, GRect(ax + 2, ay - 1, 2, 1), 0, GCornerNone);
    }
    if (offset + visible < data->count) {
        int ax = w - DM_MARGIN_X - 6;
        int ay = bounds.size.h - DM_MARGIN_Y;
        graphics_context_set_fill_color(ctx, amber);
        graphics_fill_rect(ctx, GRect(ax + 1, ay - 2, 4, 1), 0, GCornerNone);
        graphics_fill_rect(ctx, GRect(ax + 2, ay - 1, 2, 1), 0, GCornerNone);
    }
}

void transit_ui_draw_status(GContext *ctx, GRect bounds, const char *message) {
    GColor amber = (GColor){ .argb = DM_COLOR_AMBER };
    GColor black = GColorBlack;

    graphics_context_set_fill_color(ctx, black);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    char san[32];
    dm_sanitize(message, san, sizeof(san));
    int tw = dm_text_width(san, DM_HEADER_DOT);
    int th = dm_text_height(DM_HEADER_DOT);
    int x = (bounds.size.w - tw) / 2;
    int y = (bounds.size.h - th) / 2;
    dm_text(ctx, x, y, san, bounds.size.w, DM_HEADER_DOT, amber);
}
