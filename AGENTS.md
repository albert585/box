# v833_lv9_demos — Agent Reference

## Build

### Prerequisite: 3rdparty libraries (cross-compile only)

Run once before first cross-compile build:

```bash
make -C 3rdparty all
```

Builds zlib, alsa-lib, openssl, ffmpeg, libevdev with the cross-compile toolchain into `libs/`. Individual libs: `make -C 3rdparty <name>` (e.g. `alsa-lib`). Clean: `make -C 3rdparty clean`.

### Cross-compile (v833 / v853)

The binary targets ARM musl Linux (Allwinner V833/V853) and will not run on x86.

```bash
# Configure (once or after CMakeLists.txt changes)
cmake -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_setup.cmake -B build -S .
# Debug mode:
cmake -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_setup.cmake -DCMAKE_BUILD_TYPE=Debug -B build -S .
# Specific arch:
cmake -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_setup.cmake -DARCH=v853 -B build_v853 -S .

# Build
make -C build -j$(nproc)

# Pack artifacts for deployment
make -C build pack
```

- **Toolchain path**: `/usr/x-tools/arm-unknown-linux-musleabihf/` (override with `-DTOOLCHAIN_PREFIX=...`)
- **Output binary**: `build/bin/box`
- **Shared library**: `build/lib/liblvgl_linux.so` (LVGL 9.3, linked by `box`)
- **Post-build strip**: skips in Debug mode. Release builds strip `--strip-unneeded`.
- **CMake minimum**: 3.12 (required for `CONFIGURE_DEPENDS` in `file(GLOB ...)`)
- **Custom sysroot**: override via `-DSYSROOT=...` (default: `${CMAKE_SOURCE_DIR}/libs`)
- **lv_conf**: picked from `config/lv_conf_${ARCH}.h` (v833, v853, wayland)
- **Pack target**: copies `box` and `liblvgl_linux.so*` to `pack/` for deployment. `pack/run.sh` is generated from `pack/run.sh.in` and handles `LD_LIBRARY_PATH`.

### Native build (wayland)

For development/testing on x86 Linux with Wayland display server.

```bash
cmake -DARCH=wayland -DCMAKE_BUILD_TYPE=Debug -B build_wayland -S .
make -C build_wayland -j$(nproc)
```

No toolchain file needed. Dependencies resolved via pkg-config (requires: wayland-client, wayland-cursor, xkbcommon, libavcodec, libavformat, libavutil, libswscale, libswresample, alsa).

### CLI flags

| Flag | Behavior |
|------|----------|
| `-d` | Daemonize (calls `daemon(1,0)`) |
| `-w` | Watchdog mode: daemonize, switch to background, then loop reading home key only |

## Architecture

### Build targets

| Target | Type | Content |
|--------|------|---------|
| `box` | executable | `src/main.c` + `src/arch/${ARCH}/*.c` + all `src/common/**/*.c` |
| `lvgl_linux` | shared lib | LVGL 9.3 submodule (`lvgl/`) |

All application code lives in `box`. `liblvgl_linux.so` only changes when the LVGL submodule is updated.

### Layer overview

```
src/main.c             — entry point: LVGL init, evdev input, main loop, power management
src/arch/${ARCH}/      — platform abstraction (per-arch implementation)
    device.c           — device init/deinit
    display.c          — display init, LCD open/close/refresh/brightness
    input.c            — touch init, key reading (home/power)
    power.c            — deep sleep, CPU powersave/restore, LCD timeout
src/common/            — shared application code
src/main.h             — exports for power/sleep/display functions used by common modules
```

- **`parent`** is the global container object (LVGL screen-sized, 640x480), defined in `src/common/container.c:4`.
- **Event routing**: `src/common/events.c` — module transition dispatch (e.g. video playback lifecycle via `page_video()`).

### Source auto-discovery

`CMakeLists.txt` uses `file(GLOB ... CONFIGURE_DEPENDS)` on these directories. New `.c` files here are picked up automatically:

- `src/common/*.c`
- `src/common/views/*.c`
- `src/common/audio/*.c`
- `src/common/battery/*.c`
- `src/common/utils/*.c`

Arch-specific files under `src/arch/${ARCH}/*.c` are also auto-discovered.

### Key modules

| Module | Files | Purpose |
|--------|-------|---------|
| container | `src/common/container.c/h` | Root screen object (`parent`), `page_back()` |
| events | `src/common/events.c/h` | Button/playback event handlers, `page_video()` |
| button | `src/common/button.c/h` | Reusable button widget |
| audio + audio_ctrl | `src/common/audio/audio.c/h`, `audio_ctrl.c/h` | FFmpeg→ALSA PCM playback, hardware volume via ALSA Mixer |
| ff_player | `src/common/audio/ff_player.c/h` | FFmpeg-based A/V player (software decode, pthreads) |
| player | `src/common/audio/player.c/h` | UI wrapper around ff_player (video/audio playback lifecycle) |
| views | `src/common/views/lv_text_clock.c/h` | Custom LVGL text clock widget |
| battery | `src/common/battery/battery_manager.c/h` | Battery status management |
| games | `src/common/games/bird.c/h` | Flappy Bird clone (stub) |
| utils | `src/common/utils/str_utils.c/h` | String utility helpers |

### Platform abstraction (src/arch/)

Each arch provides these functions (declared in `src/arch/arch.h`):

| Function group | Purpose |
|----------------|---------|
| `arch_device_init/deinit` | Device setup/teardown |
| `arch_display_init` | LVGL display driver init |
| `arch_touch_init/open/close` | Touchscreen input |
| `arch_lcd_open/close/refresh/set_brightness/get_brightness` | LCD backlight control |
| `arch_read_key_home/power` | Hardware key polling |
| `arch_deep_sleep/cpu_powersave/cpu_restore` | Power management |
| `arch_lcd_detect_timeout` | Auto-sleep timeout check |

v853 has an additional `ls_printer.c/h` module.

## Hardware / Board specifics

Binary runs **only** on the V833/V853 (Tina Linux). Device nodes hardcoded in per-arch source:

| Device | Purpose |
|--------|---------|
| `/dev/fb0` | Framebuffer display (640x480) |
| `/dev/disp` | Display controller ioctl (LCD on/off) |
| `/dev/input/event0` | Power key (code `0x74`) |
| `/dev/input/event2` | Home key (code `0x73`, double-click = switch foreground) |
| `/dev/input/event6` | Touchscreen (evdev) |

- Power management: shallow sleep (LCD/touch off), 60s auto deep sleep (CPU powersave governor)
- Touch calibration is commented out in `main.c` — may need re-enabling per device
- Two tick functions: `custom_tick_get()` (uint64_t internal, used for keys/timeouts) and `tick_get()` (uint32_t internal, used for deep sleep timer)

## Dependencies (cross-compile sysroot)

All headers and libraries under a single sysroot (default `${CMAKE_SOURCE_DIR}/libs`):

```
libs/
├── alsa/{include,lib}/
├── ffmpeg/{include,lib}/
├── zlib/{include,lib}/
├── evdev/{include,lib}/
└── openssl/{include,lib}/
```

Override with `-DSYSROOT=/custom/path`. Compiler flags include `-mfpu=neon` for ARM NEON SIMD.

Build these from source with `make -C 3rdparty all` (requires matching cross-compile toolchain).

## LVGL

- **Submodule**: `lvgl/` — branch `release/v9.3`
- Config: `config/lv_conf_${ARCH}.h` (set via `LV_BUILD_CONF_PATH`). The root `lv_conf.h` is a reference copy (not used in builds).
- LVGL 9.x — no `lv_drv_conf.h`; driver config is inline in `lv_conf.h`
- Uses FBDEV backend (not DRM). Resolution: 640x480 (v833), 1280x768 (v853).

## Other notes

- **No tests, no linter, no formatter** configured in this repo
- `include/cdc_ion_5.4.h` is a vendor-supplied header; not used in the build
- `.gitignore` excludes `build*`, `pack`, `libs`, `reverse`, `.cache`, `AGENTS.md`, `*.backup`, and `*.log`
