#include "dm_font.h"
#include <ctype.h>
#include <string.h>

// 5x7 dot-matrix font — ported from Bildschirmli SmallTV-Ultra.
// Each row is a 5-bit bitmask (MSB = leftmost column).
const uint8_t DM_FONT_5X7[128][7] = {
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['-'] = {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00},
    ['\'']= {0x06, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00},
    [':'] = {0x00, 0x06, 0x06, 0x00, 0x06, 0x06, 0x00},
    ['.'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06},
    ['/'] = {0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10},
    ['0'] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
    ['1'] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    ['2'] = {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F},
    ['3'] = {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},
    ['4'] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
    ['5'] = {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    ['6'] = {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},
    ['7'] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    ['8'] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    ['9'] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
    ['A'] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    ['B'] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
    ['C'] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},
    ['D'] = {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C},
    ['E'] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},
    ['F'] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
    ['G'] = {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E},
    ['H'] = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    ['I'] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},
    ['J'] = {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C},
    ['K'] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
    ['L'] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
    ['M'] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},
    ['N'] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
    ['O'] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    ['P'] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
    ['Q'] = {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},
    ['R'] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
    ['S'] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E},
    ['T'] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    ['U'] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    ['V'] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},
    ['W'] = {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11},
    ['X'] = {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
    ['Y'] = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04},
    ['Z'] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},
};

// 8x9 bus icon — scaled from the 8x11 original (2 body rows removed).
// Preserves roof, windows, divider, body (2 rows), floor, wheel wells,
// undercarriage, wheels. Drawn at text pitch.
//
//   .██████.  0x7E  roof
//   ██....██  0xC3  windows
//   ████████  0xFF  divider
//   █......█  0x81  body
//   █......█  0x81  body
//   ████████  0xFF  floor
//   █.████.█  0xBD  wheel wells
//   ████████  0xFF  undercarriage
//   .█....█.  0x42  wheels
const uint8_t DM_BUS_ICON[9] = {
    0x7E, 0xC3, 0xFF, 0x81, 0x81, 0xFF, 0xBD, 0xFF, 0x42,
};

int dm_char_width(uint8_t dot_size) {
    int pitch = dot_size + 1;
    return 5 * pitch + 1;
}

int dm_text_width(const char *text, uint8_t dot_size) {
    int n = strlen(text);
    if (n == 0) return 0;
    return n * dm_char_width(dot_size) - 1;
}

int dm_text_height(uint8_t dot_size) {
    int pitch = dot_size + 1;
    return 7 * pitch;
}

int dm_char(GContext *ctx, int x, int y, char c, uint8_t dot_size) {
    int idx = (unsigned char)c;
    if (idx >= 128) idx = ' ';
    const uint8_t *glyph = DM_FONT_5X7[idx];

    // Check if glyph is empty (undefined char) — fall back to space
    bool empty = true;
    for (int r = 0; r < 7; r++) {
        if (glyph[r]) { empty = false; break; }
    }
    if (empty && c != ' ') glyph = DM_FONT_5X7[' '];

    int pitch = dot_size + 1;
    // Caller must set fill color before calling dm_char/dm_text

    for (int row = 0; row < 7; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 5; col++) {
            if (bits & (0x10 >> col)) {
                graphics_fill_rect(ctx,
                    GRect(x + col * pitch, y + row * pitch, dot_size, dot_size),
                    0, GCornerNone);
            }
        }
    }
    return dm_char_width(dot_size);
}

int dm_text(GContext *ctx, int x, int y, const char *text, int max_width,
            uint8_t dot_size, GColor color) {
    int cx = x;
    int len = strlen(text);
    int char_w = dm_char_width(dot_size);

    // Set color once for all characters
    graphics_context_set_fill_color(ctx, color);

    // How many chars fit?
    int max_chars = max_width / char_w;
    if (max_chars <= 0) return 0;

    bool truncated = (len > max_chars);
    int draw_count = truncated ? max_chars : len;

    for (int i = 0; i < draw_count; i++) {
        // Last position gets '.' if truncated
        if (truncated && i == draw_count - 1) {
            dm_char(ctx, cx, y, '.', dot_size);
        } else {
            dm_char(ctx, cx, y, text[i], dot_size);
        }
        cx += char_w;
    }
    return cx - x;
}

void dm_bus_icon(GContext *ctx, int x, int y, uint8_t dot_size, GColor color) {
    // Draw at text pitch (dot_size + 1) — same scale as characters.
    // 8 cols × 9 rows at pitch 2 = 16×18px on basalt, fits in 19px row.
    int pitch = dot_size + 1;
    graphics_context_set_fill_color(ctx, color);

    for (int row = 0; row < 9; row++) {
        uint8_t bits = DM_BUS_ICON[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                graphics_fill_rect(ctx,
                    GRect(x + col * pitch, y + row * pitch, dot_size, dot_size),
                    0, GCornerNone);
            }
        }
    }
}

void dm_sanitize(const char *in, char *out, size_t out_len) {
    size_t j = 0;
    const uint8_t *p = (const uint8_t *)in;

    while (*p && j < out_len - 1) {
        if (*p < 0x80) {
            out[j++] = (char)toupper((int)*p);
            p++;
        } else if (*p == 0xC3 && p[1]) {
            uint8_t c2 = p[1];
            char repl = '?';
            switch (c2) {
                case 0xA4: case 0x84: repl = 'A'; break;  // ä Ä
                case 0xB6: case 0x96: repl = 'O'; break;  // ö Ö
                case 0xBC: case 0x9C: repl = 'U'; break;  // ü Ü
                case 0xA9: case 0x89: repl = 'E'; break;  // é É
                case 0xA8: case 0x88: repl = 'E'; break;  // è È
                case 0xA0: case 0x80: repl = 'A'; break;  // à À
                default:              repl = '?'; break;
            }
            out[j++] = repl;
            p += 2;
        } else {
            // Skip unknown multi-byte
            out[j++] = '?';
            p++;
            while (*p && (*p & 0xC0) == 0x80) p++;
        }
    }
    out[j] = '\0';
}
