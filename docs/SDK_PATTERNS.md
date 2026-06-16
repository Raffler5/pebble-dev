# Pebble SDK Patterns Reference

Extracted from official `pebble-sdk-examples` and Pebble developer documentation.
These are proven patterns — every snippet comes from a working example.

---

## Window Lifecycle

### The Universal Pattern

Every example follows this skeleton:

```c
int main(void) {
    init();            // create windows, load resources, subscribe services
    app_event_loop();  // blocks until app exits
    deinit();          // destroy windows, free resources, unsubscribe
}
```

### Window Handlers

| Handler | When it fires | Used by |
|---------|--------------|---------|
| `.load` | First time window is pushed onto stack | ALL examples |
| `.appear` | Every time window becomes visible (push AND re-appear after pop) | pebble_arcade only |
| `.disappear` | Every time window leaves screen | NONE (no example uses this) |
| `.unload` | Window is popped off stack / app exits | ALL examples |

**Key rule:** `.load` fires ONCE (first push). `.appear` fires EVERY TIME the window becomes visible.

### Window Creation: Always at Init

**Every official example creates ALL windows at init time.** None create windows on-demand.

```c
// From app_font_browser — 2 windows, both created at init
static void init() {
    s_main_window = window_create();
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload,
    });

    s_font_window = window_create();
    window_set_window_handlers(s_font_window, (WindowHandlers) {
        .load = font_window_load,
        .unload = font_window_unload,
    });

    window_stack_push(s_main_window, true);  // only root pushed at init
}

static void deinit() {
    window_destroy(s_font_window);
    window_destroy(s_main_window);
}
```

**Do NOT create/destroy windows inside handlers.** This is not how the SDK was designed.

### Using `.appear` for State Reset

The only example that uses `.appear` is pebble_arcade's game window. It resets game state — not just marking dirty:

```c
static void main_window_appear(Window *window) {
    s_score = 0;
    s_time = 0;
    s_timer = NULL;
    update_time_text(MAX_TIME / 1000, (MAX_TIME % 1000) / 10);
    text_layer_set_text(s_score_text, "Press Select to Start");
}
```

---

## Multi-Window Navigation

### Push / Pop Pattern

```c
// Push a pre-created window (on user action)
static void select_click(MenuLayer *menu, MenuIndex *idx, void *ctx) {
    s_current_font = idx->row;              // store selection
    window_stack_push(s_font_window, true); // push detail view
}

// Pop happens automatically via SDK back button
// OR explicitly:
window_stack_pop(true);      // go back one window
window_stack_pop_all(true);  // exit app entirely
```

### Back Button

| Pattern | When to use |
|---------|------------|
| Don't subscribe to BUTTON_ID_BACK | SDK default pops current window (most common) |
| Explicit handler with `window_stack_pop_all(true)` | When back should exit app from any depth |
| Explicit handler with custom logic | When back needs to do cleanup first |

```c
// Explicit back handler (from tea_timer — exits app from countdown screen)
static void countdown_back_handler(ClickRecognizerRef recognizer, void *ctx) {
    window_stack_pop_all(true);
}

static void countdown_click_config(void *context) {
    window_single_click_subscribe(BUTTON_ID_BACK, countdown_back_handler);
    window_single_click_subscribe(BUTTON_ID_DOWN, countdown_cancel_handler);
}
```

**Note from docs:** Back button cannot be overridden with repeating clicks or long clicks.

### Navigation Flow Example (pebble_arcade)

```
game (pushed at init, .appear resets state)
  → [timer expires] → score (pushed)
    → [high score?] → entry (pushed) → [back] → score → [back] → game
  → [back from score] → game (.appear fires, game resets)
```

---

## Click Config Providers

### Three Variants

```c
// Variant A: Simple — set on window
window_set_click_config_provider(window, my_click_config_provider);

// Variant B: With context data
window_set_click_config_provider_with_context(window, provider, my_data);

// Variant C: Delegated to a layer (MenuLayer, ScrollLayer)
menu_layer_set_click_config_onto_window(menu_layer, window);
scroll_layer_set_click_config_onto_window(scroll_layer, window);
```

### Click Config Inside .load (tea_timer pattern)

```c
static void countdown_window_load(Window *window) {
    window_set_click_config_provider(window, countdown_click_config_provider);
    // ... create layers ...
}
```

---

## Custom Layers

### Basic Custom Layer

```c
static void my_update_proc(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);
    // ... your drawing code ...
}

// In window_load:
s_my_layer = layer_create(bounds);
layer_set_update_proc(s_my_layer, my_update_proc);
layer_add_child(window_layer, s_my_layer);

// In window_unload:
layer_destroy(s_my_layer);
```

### Layer with Attached Data

```c
typedef struct {
    unsigned int progress;
} ProgressData;

// Create with extra memory:
s_bar = layer_create_with_data(bounds, sizeof(ProgressData));
layer_set_update_proc(s_bar, bar_update_proc);

// Read data inside update_proc:
static void bar_update_proc(Layer *layer, GContext *ctx) {
    ProgressData *data = (ProgressData *)layer_get_data(layer);
    // use data->progress
}

// Modify data from outside (timer, click handler):
ProgressData *d = layer_get_data(s_bar);
d->progress += 10;
layer_mark_dirty(s_bar);  // trigger redraw
```

### Drawing on Root Layer Directly

```c
// No layer_create needed — use the window's root layer
static void window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    layer_set_update_proc(root, my_update_proc);
}
```

### When to Call layer_mark_dirty()

| Trigger | Examples |
|---------|---------|
| Timer callback | layer_data, frame_buffer, timer_animation, accel_discs |
| Click handler | layer_data, gpath |
| Focus handler | focus_handler |
| After modifying layer data | Any time data changes that affects drawing |
| Never (static content) | draw_bitmap |

**Important:** `layer_mark_dirty()` schedules a redraw — it doesn't draw immediately. The update_proc fires later in the event loop. If a redraw is already pending, the call is a no-op.

---

## Timer Patterns

### One-Shot Timer

```c
s_timer = app_timer_register(1500, timer_callback, NULL);

static void timer_callback(void *data) {
    // Do something once. Do NOT re-register.
}
```

### Repeating Timer (Self-Re-registering)

```c
#define INTERVAL_MS 50

static void timer_callback(void *data) {
    // Update state
    layer_mark_dirty(s_my_layer);
    // Re-register for next tick
    app_timer_register(INTERVAL_MS, timer_callback, NULL);
}

// Start in init() or window_load:
app_timer_register(INTERVAL_MS, timer_callback, NULL);
```

### Conditional Repeating Timer

```c
static void timer_callback(void *data) {
    ProgressData *d = layer_get_data(s_bar);
    if (d->progress > 129) {
        s_timer = NULL;  // stop
    } else {
        layer_mark_dirty(s_bar);
        s_timer = app_timer_register(SPEED, timer_callback, NULL);
    }
}
```

### Drift-Compensated Timer (pebble_arcade)

```c
static void timer_callback(void *data) {
    uint16_t c_ms = time_ms(NULL, NULL);
    uint16_t elapsed = /* measure actual elapsed with wrap handling */;
    s_timer = app_timer_register(INTERVAL - (elapsed % INTERVAL), timer_callback, NULL);
}
```

---

## Accelerometer

### Tap Detection (Wrist Shake)

```c
// Subscribe:
accel_tap_service_subscribe(tap_handler);

static void tap_handler(AccelAxisType axis, int32_t direction) {
    // Wrist shake detected — refresh, toggle, etc.
}

// Unsubscribe in deinit or window_unload:
accel_tap_service_unsubscribe();
```

### Continuous Polling

```c
// Subscribe with 0 samples (use peek):
accel_data_service_subscribe(0, NULL);

// Read in timer callback:
AccelData accel = { .x = 0, .y = 0, .z = 0 };
accel_service_peek(&accel);
// Use accel.x, accel.y, accel.z

// Unsubscribe:
accel_data_service_unsubscribe();
```

---

## Persistent Storage

```c
#define MY_KEY 1
#define MY_DEFAULT 0

// Read on init:
int value = persist_exists(MY_KEY) ? persist_read_int(MY_KEY) : MY_DEFAULT;

// Write on deinit:
persist_write_int(MY_KEY, value);
```

Available types: `persist_read_int/write_int`, `persist_read_string/write_string`, `persist_read_data/write_data`.

Storage survives app restarts but not app reinstalls.

---

## MenuLayer

### Full MenuLayer (Custom Drawing)

```c
s_menu_layer = menu_layer_create(bounds);
menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_sections = menu_get_num_sections_callback,
    .get_num_rows = menu_get_num_rows_callback,
    .get_header_height = menu_get_header_height_callback,
    .draw_header = menu_draw_header_callback,
    .draw_row = menu_draw_row_callback,
    .select_click = menu_select_callback,
});
menu_layer_set_click_config_onto_window(s_menu_layer, window);
layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));
```

### SimpleMenuLayer (Declarative)

```c
SimpleMenuItem items[] = {
    { .title = "Item 1", .callback = item1_callback },
    { .title = "Item 2", .subtitle = "Details", .callback = item2_callback },
};
SimpleMenuSection sections[] = {
    { .num_items = 2, .items = items },
};
// Note: window passed directly — handles click binding internally
s_menu = simple_menu_layer_create(bounds, window, sections, 1, NULL);
layer_add_child(window_layer, simple_menu_layer_get_layer(s_menu));
```

### Updating Menu Content

```c
// After modifying data:
layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
// Or for SimpleMenuLayer:
layer_mark_dirty(simple_menu_layer_get_layer(s_menu));
```

---

## ScrollLayer

```c
s_scroll_layer = scroll_layer_create(bounds);
scroll_layer_set_click_config_onto_window(s_scroll_layer, window);

// Content goes inside the scroll layer:
s_text_layer = text_layer_create(GRect(0, 0, bounds.size.w, 2000));
text_layer_set_text(s_text_layer, long_text);

// Trim to actual content size:
GSize content_size = text_layer_get_content_size(s_text_layer);
text_layer_set_size(s_text_layer, content_size);
scroll_layer_set_content_size(s_scroll_layer,
    GSize(bounds.size.w, content_size.h + 4));

scroll_layer_add_child(s_scroll_layer, text_layer_get_layer(s_text_layer));
layer_add_child(window_layer, scroll_layer_get_layer(s_scroll_layer));
```

---

## Focus Service

Different from `.appear`/`.disappear` — fires when app gains/loses focus (e.g., notification modal):

```c
app_focus_service_subscribe(focus_handler);

static void focus_handler(bool in_focus) {
    if (in_focus) {
        // App regained focus — refresh display
    } else {
        // App lost focus — pause timers, etc.
    }
}
```

---

## Memory Management Rules

| Resource | Create in | Destroy in |
|----------|-----------|------------|
| `Window` | `init()` | `deinit()` |
| `Layer`, `TextLayer`, `MenuLayer`, `ScrollLayer` | `.load` | `.unload` |
| `GBitmap` | `.load` | `.unload` |
| `GPath` | `init()` | `deinit()` |
| `malloc`'d data | `init()` | `deinit()` |
| `AppTimer` | Event handler | Stops by not re-registering |

---

## GContext Drawing Functions

| Category | Functions |
|----------|----------|
| Fill | `graphics_fill_rect`, `graphics_fill_circle` |
| Stroke | `graphics_draw_round_rect`, `graphics_draw_line`, `graphics_draw_pixel` |
| Color | `graphics_context_set_fill_color`, `graphics_context_set_stroke_color` |
| Text | `graphics_draw_text` |
| Bitmap | `graphics_draw_bitmap_in_rect` |
| Path | `gpath_draw_filled`, `gpath_draw_outline` |
| Framebuffer | `graphics_capture_frame_buffer`, `graphics_release_frame_buffer` |

---

## Data Passing to update_proc

| Method | When to use |
|--------|------------|
| File-scope static globals | Single-instance layers (most common, simplest) |
| `layer_create_with_data()` + `layer_get_data()` | Encapsulated, reusable layer components |
| `window_set_user_data()` + `window_get_user_data()` | Per-window custom data (rare) |
| MenuLayer `callback_context` | Passing data arrays to menu callbacks |

---

*Source: pebble-sdk-examples (github.com/pebble/pebble-sdk-examples)*
*Extracted: 2026-06-16*
