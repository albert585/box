# Development Guide

## Quick Start (Wayland)

```bash
cmake -DARCH=wayland -DCMAKE_BUILD_TYPE=Debug -B build_wayland -S .
make -C build_wayland -j$(nproc)
./build_wayland/bin/box
```

Requires: `wayland-client`, `wayland-cursor`, `xkbcommon`, `libavcodec`, `libavformat`, `libavutil`, `libswscale`, `libswresample`, `alsa`.

---

## Container System

### `Con` — Basic Container Config

```c
typedef struct {
    lv_obj_t *object;                // parent object (usually lv_screen_active() or lv_scr_act())
    int32_t   width;                 // container width
    int32_t   height;                // container height
    uint32_t  color;                 // background color (0 = skip)
    lv_opa_t  opa;                   // background opacity
    lv_align_t align;                // alignment (0 = skip)
    lv_scrollbar_mode_t scrollbar_mode;
} Con;
```

### `create_container(Con *con)` → `lv_obj_t *`

Creates a styled LVGL container. Handles size, background, border (always 0), alignment, scrollbar.

```c
Con con = {
    .object = lv_screen_active(),
    .width  = 640,
    .height = 480,
    .opa    = LV_OPA_COVER,
    .scrollbar_mode = LV_SCROLLBAR_MODE_OFF,
};
lv_obj_t *box = create_container(&con);
```

### `create_close_button(parent, cb, user_data)` → `lv_obj_t *`

Adds an "x" close button to `parent` (top-right corner). Calls `cb` on click with `user_data`. Must be called **after** child widgets to stay on top.

---

## Page System

### `PageCon` — Full-Page Container

```c
typedef void (*page_delete_cb_t)(void *data);

typedef struct {
    Con   base;                      // inherited container config
    page_delete_cb_t on_delete;      // cleanup callback (NULL = none)
    void *user_data;                 // passed to on_delete
    bool  closable;                  // add close button (top-right "x")
} PageCon;
```

### `create_page(PageCon *pc)` → `lv_obj_t *`

Creates a page container and manages page lifecycle automatically:

1. Creates the container via `create_container`
2. Hides `main_page` (`lv_obj_add_flag(main_page, LV_OBJ_FLAG_HIDDEN)`)
3. If `closable == true`, adds close button pointing to `container_close_cb`
4. Registers `LV_EVENT_DELETE` handler that calls `on_delete(user_data)` then restores `main_page`

**Lifecycle**: enter → `main_page` hidden → user clicks × → `container_close_cb` → `lv_obj_del(page)` → `LV_EVENT_DELETE` → `on_delete(user_data)` → `main_page` restored.

### Example

```c
static void my_cleanup(void *data) {
    MyState *s = (MyState *)data;
    lv_timer_del(s->timer);
    free(s);
}

void open_my_page(void) {
    MyState *state = calloc(1, sizeof(MyState));

    PageCon pc = {
        .base = {
            .object = lv_scr_act(),
            .width  = 480,
            .height = 320,
            .opa    = LV_OPA_COVER,
            .scrollbar_mode = LV_SCROLLBAR_MODE_OFF,
        },
        .on_delete = my_cleanup,
        .user_data = state,
        .closable  = true,
    };
    lv_obj_t *page = create_page(&pc);

    // add child widgets...
}
```

---

## Adding a New Page

1. Create `src/common/xxx/xxx_page.c` and `xxx_page.h`
2. Define cleanup callback (`void my_cleanup(void *data)`)
3. In entry function, build `PageCon` and call `create_page`
4. Add child widgets to the returned `lv_obj_t *`

```c
// xxx_page.h
void open_xxx_page(void);

// xxx_page.c
#include "xxx_page.h"
#include "container.h"

static void xxx_cleanup(void *data) {
    /* release resources */
    free(data);
}

void open_xxx_page(void) {
    void *state = calloc(1, sizeof(MyState));

    PageCon pc = {
        .base = { .object = lv_scr_act(), .width = 480, .height = 320,
                  .scrollbar_mode = LV_SCROLLBAR_MODE_OFF },
        .on_delete = xxx_cleanup,
        .user_data = state,
        .closable  = true,
    };
    lv_obj_t *page = create_page(&pc);

    /* add widgets to `page` ... */
}
```

---

## Adding a New Architecture

### Directory Layout

```
src/arch/
├── arch.h                          # shared declarations (unchanged)
├── armhf/chips/{v833,v853,v853s}/  # ARM embedded chips
└── x86_64/wayland/                 # native Wayland
```

### Per-Arch Files

Each arch directory must implement these functions (declared in `src/arch/arch.h`):

| Function | Purpose |
|----------|---------|
| `arch_device_init/deinit` | Open/close device nodes |
| `arch_display_init` | Create LVGL display driver |
| `arch_touch_init/open/close` | Touch input |
| `arch_lcd_open/close/refresh` | LCD power control |
| `arch_lcd_set_brightness/get_brightness` | Backlight |
| `arch_timer_handler` | Per-frame update (wayland: `lv_wayland_timer_handler`, embedded: `lv_timer_handler` + timeout detect). Returns ms until next call. |
| `arch_read_key_home/power` | Hardware key polling |
| `arch_deep_sleep/cpu_powersave/cpu_restore` | Power management |
| `arch_lcd_detect_timeout` | Auto-sleep timeout check |

### CMakeLists.txt Changes

- `LV_CONF_ARCH` mapping (e.g. v853s → v853)
- `ARCH_SOURCE_DIR` path mapping (`armhf/chips/` or `x86_64/`)

### lv_conf

Place in `config/lv_conf_${ARCH}.h`. If reusing another arch's config, set `LV_CONF_ARCH` override in CMakeLists.txt.

---

## Adding a New Source Directory

Edit `CMakeLists.txt`, add a line to `file(GLOB BOX_SOURCES ...)`:

```cmake
file(GLOB BOX_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/*.c
    ...
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/your_new_dir/*.c   # ADD THIS
)
```

---

## Theme Configuration

Theme is controlled per-arch via `config/lv_conf_${ARCH}.h`:

```c
#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT
    #define LV_THEME_DEFAULT_DARK 1   // 0 = light, 1 = dark
    #define LV_THEME_DEFAULT_GROW 1
    #define LV_THEME_DEFAULT_TRANSITION_TIME 80
#endif
```

---

## Cross-Compile

```bash
# Build 3rdparty libs (first time only)
make -C 3rdparty all

# Configure and build
cmake -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_setup.cmake -DARCH=v853 -B build -S .
make -C build -j$(nproc)

# Pack for deployment
make -C build pack
```

Supported `-DARCH` values: `v833`, `v853`, `v853s`. Note: `v853s` reuses `lv_conf_v853.h`.
