# v833_lv9_demos — Agent Reference

## Build

**Cross-compilation only.** The binary targets ARM musl Linux (Allwinner V833) and will not run on x86.

```bash
# Configure (once or after CMakeLists.txt changes)
cmake -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_setup.cmake -B build -S .
# FFmpeg 4.x variant:
cmake -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_setup.cmake -DFFMPEG_VERSION=4 -B build -S .
# Debug mode:
cmake -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_setup.cmake -DCMAKE_BUILD_TYPE=Debug -B build -S .

# Build
make -C build -j$(nproc)
```

- **Toolchain path**: `/usr/x-tools/arm-unknown-linux-musleabihf/` (in `user_cross_compile_setup.cmake`)
- **Output binary**: `build/bin/lvglsim` (auto-stripped of debug symbols post-build)
- **Clean rebuild**: `make -C build clean && make -C build -j$(nproc)`

## Architecture

- **Entry point**: `src/main.c` — LVGL init, framebuffer display, evdev input, main loop, power management
- **Module lib**: `src/lib/` — compiled into `lvgl_linux` shared library, then linked to `lvglsim`
- **Event routing**: `src/lib/events.c` — module transition dispatch
- **Container system**: `src/lib/container.c` — main screen layout; `parent` is the global container object

### Source auto-discovery

`CMakeLists.txt` uses `file(GLOB ...)` on `src/lib/*.c` and `src/lib/views/*.c`. New `.c` files dropped in these dirs are picked up automatically.

### Key modules

| Module | Files | Purpose |
|--------|-------|---------|
| audio + audio_ctrl | `audio.c/h`, `audio_ctrl.c/h` | FFmpeg→ALSA PCM playback, hardware volume via ALSA Mixer |
| ff_player | `ff_player.c/h` | FFmpeg-based A/V player (software decode) |
| player | `player.c/h` | UI wrapper around ff_player (video/audio playback) |
| views | `views/` | Custom LVGL widgets (e.g. `lv_text_clock`) |

## Hardware / Board specifics

Binary runs **only** on the V833 (Tina Linux). Device nodes hardcoded in `src/main.c`:

| Device | Purpose |
|--------|---------|
| `/dev/fb0` | Framebuffer display (640×480) |
| `/dev/disp` | Display controller ioctl (LCD on/off) |
| `/dev/input/event0` | Power key (code `0x74`) |
| `/dev/input/event2` | Home key (code `0x73`, double-click = switch foreground) |
| `/dev/input/event6` | Touchscreen (evdev) |

- Power management: shallow sleep (LCD/touch off), 60s auto deep sleep (CPU powersave governor)
- Touch calibration is commented out in main.c — may need re-enabling per device

## Dependencies (cross-compile sysroot)

Headers/libs at absolute paths in `/srv/` — **must exist on the build machine**:

- `/srv/alsa/` — ALSA
- `/srv/ffmpeg/` — FFmpeg 6.x (default)
- `/srv/ffmpeg-ssd/` — FFmpeg 4.x (use `-DFFMPEG_VERSION=4`)
- `/srv/evdev/` — evdev
- `/srv/openssl/` — OpenSSL
- `/srv/zlib-armv7/`, `/srv/zlib/` — zlib

## LVGL

- **Submodule**: `lvgl/` — fork at https://github.com/albert585/lvgl, branch `release/v9.3`
- Config: repo root `lv_conf.h` (not inside lvgl/). LVGL picks it up via `LV_BUILD_CONF_PATH`.
- LVGL 9.x — no `lv_drv_conf.h`; driver config is inline in `lv_conf.h`
- Uses FBDEV backend (not DRM). Resolution: 640×480.
