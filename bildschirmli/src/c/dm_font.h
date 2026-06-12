#pragma once

#include <pebble.h>

// 5x7 dot-matrix font — uppercase ASCII + digits + punctuation.
// Lowercase / accents must be folded by dm_sanitize() before drawing.
extern const uint8_t DM_FONT_5X7[128][7];

// 8x11 bus icon bitmap (drawn when ETA = "NOW")
extern const uint8_t DM_BUS_ICON[11];

// Dot sizes — platform-adaptive
#ifdef PBL_PLATFORM_EMERY
  #define DM_HEADER_DOT  3
  #define DM_ROW_DOT     2
  #define DM_MARGIN_X    6
  #define DM_MARGIN_Y    6
  #define DM_ROW_GAP     4
#else
  #define DM_HEADER_DOT  2
  #define DM_ROW_DOT     1
  #define DM_MARGIN_X    4
  #define DM_MARGIN_Y    4
  #define DM_ROW_GAP     3
#endif

// Amber phosphor color — closest to 0xFBE0 in Pebble's 64-color palette
#define DM_COLOR_AMBER GColorChromeYellowARGB8

// Character metrics
int dm_char_width(uint8_t dot_size);
int dm_text_width(const char *text, uint8_t dot_size);
int dm_text_height(uint8_t dot_size);

// Drawing
int dm_char(GContext *ctx, int x, int y, char c, uint8_t dot_size, GColor color);
int dm_text(GContext *ctx, int x, int y, const char *text, int max_width, uint8_t dot_size, GColor color);
void dm_bus_icon(GContext *ctx, int x, int y, uint8_t dot_size, GColor color);

// Sanitize: uppercase, fold UTF-8 umlauts/accents to ASCII
void dm_sanitize(const char *in, char *out, size_t out_len);
