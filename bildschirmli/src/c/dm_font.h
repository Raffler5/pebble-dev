#pragma once

#include <pebble.h>

// 5x7 dot-matrix font — uppercase ASCII + digits + punctuation.
// Lowercase / accents must be folded by dm_sanitize() before drawing.
extern const uint8_t DM_FONT_5X7[128][7];

// 8x8 bus icon — scaled from 8x11 original (2 body rows + windows removed).
// Drawn at text pitch (DM_ROW_DOT+1). 8 rows × pitch 2 = 16px on basalt,
// fits in 19px row height with 3px clearance.
#define DM_BUS_ROWS 8
#define DM_BUS_COLS 8
extern const uint8_t DM_BUS_ICON[DM_BUS_ROWS];

// Dot sizes — platform-adaptive
#ifdef PBL_PLATFORM_EMERY
  #define DM_HEADER_DOT  2
  #define DM_ROW_DOT     1
  #define DM_MARGIN_X    6
  #define DM_MARGIN_Y    6
  #define DM_ROW_GAP     3
#else
  #define DM_HEADER_DOT  2
  #define DM_ROW_DOT     1
  #define DM_MARGIN_X    4
  #define DM_MARGIN_Y    4
  #define DM_ROW_GAP     3
#endif

// Defaults — GColorChromeYellow text on GColorBlack background
#define DM_COLOR_DEFAULT    0xF8  // GColorChromeYellow
#define DM_BG_COLOR_DEFAULT 0xC0  // GColorBlack

// Runtime colors — set from settings, used by all drawing
extern uint8_t dm_color_argb8;
extern uint8_t dm_bg_color_argb8;

// Convenience macros
#define DM_COLOR_CURRENT() ((GColor){ .argb = dm_color_argb8 })
#define DM_BG_CURRENT()    ((GColor){ .argb = dm_bg_color_argb8 })

// Character metrics
int dm_char_width(uint8_t dot_size);
int dm_text_width(const char *text, uint8_t dot_size);
int dm_text_height(uint8_t dot_size);

// Drawing — dm_text sets fill color; dm_char expects caller to set it
int dm_char(GContext *ctx, int x, int y, char c, uint8_t dot_size);
int dm_text(GContext *ctx, int x, int y, const char *text, int max_width, uint8_t dot_size, GColor color);
void dm_bus_icon(GContext *ctx, int x, int y, uint8_t dot_size, GColor color);

// Sanitize: uppercase, fold UTF-8 umlauts/accents to ASCII
void dm_sanitize(const char *in, char *out, size_t out_len);
