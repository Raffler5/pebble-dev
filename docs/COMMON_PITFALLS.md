# Common Pitfalls & Hard-Won Lessons

Problems we hit during Bildschirmli development and their root causes.
This document exists to prevent repeating these mistakes.

---

## 1. The Black Screen Bug (Layer Lifecycle)

**Symptom:** After pressing Back from a pushed window, the underlying window shows a black screen. Buttons still work (blindly), but nothing is drawn.

**Root cause:** Creating a child layer in `.load` and destroying it in `.unload` seems correct, but it breaks on **second push**. Here's why:

```
1st push: .load fires → layer_create → layer_add_child → layer_set_update_proc ✓
Back:     .unload fires → layer_destroy ✓
2nd push: .load fires AGAIN → layer_create → layer_add_child → ...
          But the root layer's child list may be corrupted from the destroy/recreate cycle
```

The root layer keeps internal state about its children. Destroying and recreating a child across push/pop cycles can leave orphaned references in the layer tree.

**Fix:** Don't create child layers at all. Set `layer_set_update_proc()` directly on the window's root layer:

```c
// WRONG — child layer lifecycle causes black screen on 2nd push
static void window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    s_my_layer = layer_create(bounds);            // ← problem
    layer_set_update_proc(s_my_layer, my_draw);
    layer_add_child(root, s_my_layer);
}
static void window_unload(Window *window) {
    layer_destroy(s_my_layer);                    // ← creates orphan
}

// RIGHT — root layer is managed by the window, no lifecycle issues
static void window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    layer_set_update_proc(root, my_draw);         // ← just set the proc
}
static void window_unload(Window *window) {
    // nothing to destroy
}
```

This matches the `feature_frame_buffer` pattern from official examples.

**Other things we tried that did NOT fix it:**
- `.appear` handler with `layer_mark_dirty()` — no-op if redraw already pending
- Delayed timer (100ms) to mark dirty after transition — unreliable
- Creating window fresh on each push, destroying in unload (SwissTP pattern) — same child layer bug
- Marking both custom layer and root layer dirty — still black

**Only fix that works:** Don't use child layers for custom drawing. Use root layer directly.

---

## 2. Truncation Dots Appearing Everywhere

**Symptom:** Every text field shows a trailing "." even when the text should fit.

**Root cause v1:** The truncation check `cx + 5 * pitch + char_w > x + max_width` was checking if the *current char plus one more* would overflow. This triggers one character too early.

**Root cause v2:** Even after fixing v1, the "clear and overwrite last char with dot" logic used `graphics_fill_rect(ctx, rect, GColorBlack)` to erase the last character. On **inverted rows** (amber background, black text), this punched a visible black hole instead of blending with the amber background.

**Fix:** Simple char-counting approach:
```c
int max_chars = max_width / char_w;
bool truncated = (len > max_chars);
int draw_count = truncated ? max_chars : len;

for (int i = 0; i < draw_count; i++) {
    if (truncated && i == draw_count - 1) {
        dm_char(ctx, cx, y, '.', dot_size);  // replace last with dot
    } else {
        dm_char(ctx, cx, y, text[i], dot_size);
    }
    cx += char_w;
}
```

No clearing needed — the dot simply replaces the last character in the same color.

---

## 3. Bus Icon Overflowing Into Adjacent Rows

**Symptom:** The bus icon drawn for ETA=0 overlaps with the text in the row below.

**Root cause:** The original 8×11 bitmap drawn at text pitch (DM_ROW_DOT+1=2) is 22px tall. The text row is only 14px (7×2). Even with the 5px gap between rows (total row height 19px), the icon overflows by 3px into the next row.

**The math:**
```
Icon height = icon_rows × pitch
Text height = 7 × pitch
Row height  = text_height + DM_ROW_GAP + 2

If icon_rows × pitch > row_height, the icon overflows.

Basalt (pitch=2, gap=3): row_height = 14+3+2 = 19px
  11 rows × 2 = 22px → overflow 3px ✗
   9 rows × 2 = 18px → overflow? barely fits, 1px clearance — still overlaps visually
   8 rows × 2 = 16px → 3px clearance ✓
```

**Fix:** Redesign the icon to fit. We went from 8×11 → 8×8 (removed 2 body rows + 1 window row). The remaining 8 rows preserve: roof, solid top, body (2), floor, wheel wells, undercarriage, wheels.

**Rule:** `icon_rows × pitch ≤ row_height - 2` for safe clearance.

---

## 4. Column Layout Too Tight for Station Names

**Symptom:** Station names truncated to 2-3 characters ("UN.", "DO."). Directions show as "B." (unreadable).

**Root cause:** Column allocation was designed for big screens but tested on basalt (144px wide). At dotSize 1, each character is 11px wide. With 136px usable width:

```
Old allocation:
  Line: 4 chars × 11px = 44px
  ETA:  5 chars × 11px = 55px
  Dir:  136 - 44 - 55 - 4 = 33px ≈ 3 chars ← useless

Fixed allocation:
  Line: 3 chars × 11px = 33px
  Gap:  1 char × 11px  = 11px
  ETA:  3 chars × 11px = 33px
  Dir:  136 - 33 - 11 - 33 = 59px ≈ 5 chars ← usable
```

**Also:** The header had station name + time side by side. "HH:MM" (5 chars × 11px = 55px) left only ~70px for the station name. Fix: removed time from header entirely — it's redundant on a watch with relative departure minutes.

**Rule:** Always calculate pixel budgets on the **smallest** target platform first. Design for basalt (144px), then let emery (200px) benefit from the extra space.

---

## 5. `graphics_context_set_fill_color` Called Per Character

**Symptom:** No visible bug, but unnecessary overhead — 35 `set_fill_color` calls for a 5-character word (7 rows × 5 cols checked per char, fill_color set before each char).

**Fix:** Set fill color once in `dm_text()`, not in each `dm_char()` call. `dm_char()` now expects the caller to have set the color.

---

## 6. C99 Designated Initializers in .c Files

The font table uses C99 designated initializers:
```c
const uint8_t DM_FONT_5X7[128][7] = {
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['A'] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    // ...
};
```

This is valid C99 but **not valid C++**. Pebble SDK compiles `.c` files as C, so this works. But if you ever rename the file to `.cpp` or include it from C++, it will fail.

**Rule:** Keep font data in `.c` files, not `.cpp`.

---

*Accumulated during Bildschirmli Pebble development, June 2026*
