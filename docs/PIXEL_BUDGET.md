# Pixel Budget & Dot-Matrix Layout Math

How to calculate what fits on Pebble's small displays with our custom dot-matrix font.

---

## Font Metrics

The 5×7 dot-matrix font renders each character as a grid of filled squares.

| Parameter | Formula | Basalt (dotSize=1) | Emery (dotSize=2) |
|-----------|---------|--------------------|--------------------|
| **Pitch** | dotSize + 1 | 2 | 3 |
| **Char width** | 5 × pitch + 1 | 11px | 16px |
| **Char height** | 7 × pitch | 14px | 21px |
| **Chars per row** | usable_width / char_width | ~12 | ~11 |

The `+1` in char width is the inter-character gap (1px).

---

## Screen Budget

| | Basalt (144×168) | Emery (200×228) |
|---|---|---|
| **Margins** | 4px each side | 6px each side |
| **Usable width** | 136px | 188px |
| **Usable height** | 160px | 216px |
| **Header dot size** | 2 (same as emery row) | 3 |
| **Row dot size** | 1 | 2 |
| **Header height** | 14 + 3 + 4 = 21px | 21 + 4 + 4 = 29px |
| **Row height** | 14 + 3 + 2 = 19px | 21 + 4 + 2 = 27px |
| **Rows that fit** | ~7 | ~7 |

Both platforms fit roughly the same number of rows because emery's bigger screen is offset by its bigger dot size.

---

## Column Allocation (Departure Board)

Design for basalt first (tightest), then emery benefits.

**Basalt (136px usable, 11px per char):**

```
Line:   3 chars × 11px = 33px    "S2", "520", "13"
Gap:    1 char  × 11px = 11px    visual separator
Dir:    rest            = 59px    ~5 chars "BULACH", "BHF"
ETA:    3 chars × 11px = 33px    "6'", "21'", "99'"
                        ------
Total:                   136px
```

**Key insight:** Line numbers are 2-3 chars. ETAs are 1-3 chars. Give direction everything else.

### What Didn't Work

```
Line: 4 chars = 44px   ← wasted 1 char
ETA:  5 chars = 55px   ← wasted 2 chars
Dir:  136-44-55-4 = 33px ≈ 3 chars ← unreadable "B."
```

**Rule:** Measure the actual data, not the theoretical maximum. "520" is always 3 chars. "21'" is always 3 chars. Don't allocate for edge cases that never happen.

---

## Station Picker Columns

```
Name: usable_width - dist_col - gap
Dist: 3 chars (plain meters, no "M" suffix)

Basalt: name = 136 - 33 - 4 = 99px ≈ 9 chars
```

9 chars is enough for "UNTERDORF" (9), "BHF" (3), "ROSENWIES." (truncated at 9).

**The "M" suffix was eating a char slot.** Dropping it saved 11px for the station name.

---

## Header Layout

**Problem:** Station name + time "HH:MM" side by side.
- Time: 5 chars × 11px = 55px (at header dot size)
- Gap: ~11px
- Name: 136 - 55 - 11 = 70px ≈ 6 chars at header dot size
- "UNTERDORF" = 9 chars → truncated to "UN." (useless)

**Fix:** Remove time from header. Station name gets full width.
- Name: 136px ≈ 12 chars at header dot size → "UNTERDORF" fits

Time is redundant — it's a watch, and departures show relative minutes.

---

## Icon Sizing

Icons must fit within `row_height` to avoid overlapping adjacent rows.

**Constraint:** `icon_rows × pitch ≤ row_height - 2`

| Platform | Pitch | Row height | Max icon rows | Usable |
|----------|-------|------------|---------------|--------|
| Basalt | 2 | 19px | floor(17/2) = 8 | 8 |
| Emery | 3 | 27px | floor(25/3) = 8 | 8 |

Our 8×8 bus icon at pitch 2 = 16px, leaving 3px clearance. Verified no overlap.

**What didn't work:**
- 8×11 at pitch 2 = 22px → 3px overflow ✗
- 8×11 at pitch 1 = 11px → fits but too tiny (8×11 pixels) ✗
- 8×9 at pitch 2 = 18px → 1px clearance, still overlapped visually ✗
- 8×8 at pitch 2 = 16px → 3px clearance ✓

---

## Platform-Adaptive Design

Use `#ifdef PBL_PLATFORM_EMERY` for platform-specific values:

```c
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
```

**Never hardcode pixel positions.** Always derive from `layer_get_bounds()`, `dm_char_width()`, `dm_text_height()`.

---

## Quick Reference: "Will This Fit?"

To check if N characters of text fit in W pixels:

```
fits = (N × dm_char_width(dot_size) - 1) ≤ W
```

The `-1` accounts for no trailing gap on the last character.

To check how many characters fit in W pixels:

```
max_chars = W / dm_char_width(dot_size)
```

(Integer division — floor automatically.)

---

*Calculated and verified during Bildschirmli development on basalt (144×168), June 2026*
