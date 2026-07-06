# v833_lv9_demos — Agent Reference

## Build

**Cross-compilation only.** The binary targets ARM musl Linux (Allwinner V833) and will not run on x86.

```bash
# Configure (once or after CMakeLists.txt changes)
cmake -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_setup.cmake -B build -S .
# Debug mode:
cmake -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_setup.cmake -DCMAKE_BUILD_TYPE=Debug -B build -S .

# Build
make -C build -j$(nproc)
```

- **Toolchain path**: `/usr/x-tools/arm-unknown-linux-musleabihf/` (configurable via `-DTOOLCHAIN_PREFIX=...`)
- **Output binary**: `build/bin/lvglsim`
- **Post-build strip**: skips in Debug mode. Release builds strip `--strip-unneeded`.
- **Clean rebuild**: `make -C build clean && make -C build -j$(nproc)`
- **CMake minimum**: 3.12 (required for `CONFIGURE_DEPENDS` in `file(GLOB ...)`)
- **Custom sysroot**: override via `-DSYSROOT=...` (default: `${CMAKE_SOURCE_DIR}/libs`)

### CLI flags

| Flag | Behavior |
|------|----------|
| `-d` | Daemonize (calls `daemon(1,0)`) |
| `-w` | Watchdog mode: daemonize, switch to background, then loop reading home key only |

## Architecture

- **Entry point**: `src/main.c` — LVGL init, framebuffer display, evdev input, main loop, power management
- **Module lib**: `src/lib/` — compiled into `lvgl_linux` shared library, then linked to `lvglsim`
- **Event routing**: `src/lib/events.c` — module transition dispatch (video playback lifecycle)
- **Container system**: `src/lib/container.c` — main screen layout; `parent` is the global container object
- `src/main.h` — exports for power/sleep/display functions used by lib modules

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

Override with `-DSYSROOT=/custom/path`.

- Compiler flags include `-mfpu=neon` for ARM NEON SIMD

## LVGL

- **Submodule**: `lvgl/` — fork at https://github.com/albert585/lvgl, branch `release/v9.3`
- Config: repo root `lv_conf.h` (not inside lvgl/). LVGL picks it up via `LV_BUILD_CONF_PATH`.
- LVGL 9.x — no `lv_drv_conf.h`; driver config is inline in `lv_conf.h`
- Uses FBDEV backend (not DRM). Resolution: 640×480.

## Other notes

- **No tests, no linter, no formatter** configured in this repo
- `include/cdc_ion_5.4.h` is a vendor-supplied header; not used in the build

## libMb — thermal printer image processing library

**Location**: `/usr/lib/dlam/libMb.so` on device; rootfs copy at `/tmp/rootfs_mnt/usr/lib/dlam/libMb.so`.  
**Header**: `tools/libMb.h` (reverse-engineered, complete).  
**Dependencies**: libc, libm, libpthread, libcrypto (transitive).

### Build standalone tools

```bash
make -C tools           # test_libmb + test_stbi
# Toolchain: /usr/x-tools/arm-unknown-linux-musleabihf/bin/arm-unknown-linux-musleabihf-gcc
# Link:    -L/tmp/rootfs_mnt/usr/lib/dlam -Wl,-rpath-link,/tmp/rootfs_mnt/usr/lib
```

### MbImg struct (20 bytes = 5 × int32)

```c
typedef struct {
    uint8_t *data;      // +0x00  pixel buffer (malloc'd, w*h*ch bytes)
    int      width;     // +0x04
    int      height;    // +0x08
    int      channels;  // +0x0c  1=gray, 3=RGB, 4=RGBA
    int      _reserved; // +0x10  set by CreateImg
} MbImg;
```

### Key functions (verified via Ghidra)

| Function | Signature | Notes |
|----------|-----------|-------|
| `stbi_load` | `uint8_t*(path, &w, &h, &n, desired)` | Embedded stb_image, loads PNG/JPEG/BMP |
| `stbi_image_free` | `void(buf)` | Free stbi_load'd buffer |
| `MMJ_PrinterImgBin` | `int(MbImg*, dither, size, mode)` | **In-place** dither. mode: 0=default, 1=alt. Returns 0=ok |
| `mbImg2GrayscaleData` | `void(data, w, h, mode, levels, &out_len)` | **In-place** gray→packed. mode: 0/1=gray-input, 2=RGB-input. levels: 8=binary |
| `Color2Gray` | `int(MbImg*, r1, r2)` | In-place RGB→gray (fills all 3 bytes per pixel) |
| `Gray2Color` | `int(MbImg*, channels)` | In-place gray→color (expands 1→N bytes per pixel). Buffer must be large enough for output |
| `CreateImg` | `MbImg*(w, h, ch, extra, init_byte)` | Alloc struct+data, memset with init_byte |
| `ImgCLAHE` | `void(data, w, h, clip, grid)` | Contrast-limited adaptive histogram equalization |
| `stbi_write_bmp` | `int(path, w, h, ch, data)` | Save as BMP |

### Known bugs / pitfalls

1. **`FreeImg` is broken** — calls `free(*(int*)pixel_data)` on the first 4 bytes of image data as if they were a heap pointer. Never call it. Use plain `free(img->data); free(img);` instead.
2. **Struct is 20 bytes**, not 16. Missing the 5th `_reserved` field leads to stack corruption.
3. **Ghidra first import was incomplete** — the `~Downloads/libMb_*.so` copy showed PLT stubs only. Must import from `rootfs_mnt` for real decompilation.
4. **Memory on target is tight** (128MB). Each image copy costs `w*h*3` bytes. Free promptly.

### arm-unknown-linux-musleabihf toolchain

- **Compiler**: `/usr/x-tools/arm-unknown-linux-musleabihf/bin/arm-unknown-linux-musleabihf-gcc` (or just `arm-unknown-linux-musleabihf-gcc` from PATH)
- **strip**: `/usr/x-tools/arm-unknown-linux-musleabihf/bin/arm-unknown-linux-musleabihf-strip`
- **install dir**: `/usr/x-tools/arm-unknown-linux-musleabihf/`
- **sysroot**: `${CMAKE_SOURCE_DIR}/libs` or `-DSYSROOT=...`
- **Link flags for standalone tools**: `-s -Os -mfpu=neon -Wall`
