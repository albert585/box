cmake_minimum_required(VERSION 3.12)

# Project configuration
project(v833_lv9_demos C CXX)
set(PROJECT_VERSION "1.0.0")

# CMake policies
cmake_policy(SET CMP0079 NEW)

# Build configuration
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ============================================================
# Build mode: cross-compile (default) or Wayland-native debug
# ============================================================
option(USE_WAYLAND "Build with Wayland backend for desktop debugging" OFF)

# ============================================================
# Linker flags (LDFLAGS)
# ============================================================
if(USE_WAYLAND)
    # Native build: no static libgcc/libstdc++
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()

# ============================================================
# Dependency sysroot (overridable via -DSYSROOT=... on command line)
#   Expected layout:
#     <SYSROOT>/alsa/{include,lib}
#     <SYSROOT>/ffmpeg/{include,lib}
#     <SYSROOT>/zlib/{include,lib}
#     <SYSROOT>/evdev/{include,lib}
#     <SYSROOT>/openssl/{include,lib}
# ============================================================
set(SYSROOT "${CMAKE_SOURCE_DIR}/libs" CACHE PATH
    "Cross-compile dependency sysroot")

if(NOT USE_WAYLAND)
    # Per-package paths (cross-compile only)
    set(ALSA_INCLUDE_DIR    "${SYSROOT}/alsa/include")
    set(ALSA_LIB_DIR        "${SYSROOT}/alsa/lib")
    set(FFMPEG_INCLUDE_DIR  "${SYSROOT}/ffmpeg/include")
    set(FFMPEG_LIB_DIR      "${SYSROOT}/ffmpeg/lib")
    set(ZLIB_INCLUDE_DIR    "${SYSROOT}/zlib/include")
    set(ZLIB_LIB_DIR        "${SYSROOT}/zlib/lib")
    set(EVDEV_INCLUDE_DIR   "${SYSROOT}/evdev/include")
    set(EVDEV_LIB_DIR       "${SYSROOT}/evdev/lib")
    set(OPENSSL_INCLUDE_DIR "${SYSROOT}/openssl/include")
    set(OPENSSL_LIB_DIR     "${SYSROOT}/openssl/lib")
endif()

# ============================================================
# Find host dependencies (Wayland mode only)
# ============================================================
if(USE_WAYLAND)
    find_package(PkgConfig REQUIRED)

    pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
    pkg_check_modules(WAYLAND_CURSOR REQUIRED wayland-cursor)
    pkg_check_modules(XKBCOMMON REQUIRED xkbcommon)
    pkg_check_modules(ALSA_PC alsa)
    pkg_check_modules(FFMPEG_PC libavcodec libavformat libavutil libswscale libswresample libavdevice)
    pkg_check_modules(ZLIB_PC zlib)
    pkg_check_modules(OPENSSL_PC openssl)

    # Wayland: use pkg-config for everything
    set(CROSS_COMPILE_INCLUDE_DIRS
        ${WAYLAND_CLIENT_INCLUDE_DIRS}
        ${WAYLAND_CURSOR_INCLUDE_DIRS}
        ${XKBCOMMON_INCLUDE_DIRS}
        ${ALSA_PC_INCLUDE_DIRS}
        ${FFMPEG_PC_INCLUDE_DIRS}
        ${ZLIB_PC_INCLUDE_DIRS}
        ${OPENSSL_PC_INCLUDE_DIRS}
    )
    set(FFMPEG_LIBS ${FFMPEG_PC_LIBRARIES})
    set(EVDEV_LINK "") # not needed, wayland handles input

    # ============================================================
    # Generate Wayland protocol headers
    # ============================================================
    find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

    # Find the xdg-shell protocol XML
    find_file(XDG_SHELL_XML xdg-shell.xml
        PATHS /usr/share/wayland-protocols/stable/xdg-shell
              /usr/local/share/wayland-protocols/stable/xdg-shell)

    if(NOT XDG_SHELL_XML)
        message(FATAL_ERROR "xdg-shell.xml not found. Install wayland-protocols.")
    endif()
    message(STATUS "Found xdg-shell.xml: ${XDG_SHELL_XML}")

    set(WAYLAND_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/wayland-gen)
    file(MAKE_DIRECTORY ${WAYLAND_GEN_DIR})

    # Generate wayland_xdg_shell.h (client header) - at configure time
    execute_process(
        COMMAND ${WAYLAND_SCANNER} client-header ${XDG_SHELL_XML} ${WAYLAND_GEN_DIR}/wayland_xdg_shell.h
        RESULT_VARIABLE _ret
    )
    if(NOT _ret EQUAL 0)
        message(FATAL_ERROR "wayland-scanner client-header failed")
    endif()

    # Generate wayland_xdg_shell.c (protocol glue code) - at configure time
    execute_process(
        COMMAND ${WAYLAND_SCANNER} private-code ${XDG_SHELL_XML} ${WAYLAND_GEN_DIR}/wayland_xdg_shell.c
        RESULT_VARIABLE _ret
    )
    if(NOT _ret EQUAL 0)
        message(FATAL_ERROR "wayland-scanner private-code failed")
    endif()
    message(STATUS "Generated wayland_xdg_shell.h/c in ${WAYLAND_GEN_DIR}")

    list(APPEND CROSS_COMPILE_INCLUDE_DIRS ${WAYLAND_GEN_DIR})
else()
    set(CROSS_COMPILE_INCLUDE_DIRS
        ${ALSA_INCLUDE_DIR}
        ${FFMPEG_INCLUDE_DIR}
        ${ZLIB_INCLUDE_DIR}
    )
    set(FFMPEG_LIBS -lavcodec -lavutil -lavformat -lswscale -lswresample -lavdevice)
endif()

# ============================================================
# FFmpeg (6.x only)
# ============================================================
message(STATUS "Using sysroot: ${SYSROOT}")

# ============================================================
# LVGL configuration
# ============================================================
set(LV_BUILD_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h CACHE STRING "" FORCE)
set(LV_BUILD_SET_CONFIG_OPTS ON CACHE BOOL
    "create CMAKE variables from lv_conf_internal.h" FORCE)
add_subdirectory(lvgl)

if(USE_WAYLAND)
    target_sources(lvgl PRIVATE ${WAYLAND_GEN_DIR}/wayland_xdg_shell.c)
endif()

# LVGL includes
target_include_directories(lvgl PRIVATE ${CROSS_COMPILE_INCLUDE_DIRS})

if(USE_WAYLAND)
    # Override LVGL config for Wayland native build (PUBLIC so consumers inherit)
    target_compile_definitions(lvgl PUBLIC
        USE_WAYLAND=1
        LV_USE_WAYLAND=1
        LV_USE_LINUX_FBDEV=0
        LV_USE_EVDEV=0
        LV_USE_DRAW_SW_ASM=LV_DRAW_SW_ASM_NONE
    )
else()
    target_compile_options(lvgl PRIVATE -mfpu=neon -mcpu=cortex-a7)
    target_compile_definitions(lvgl PUBLIC USE_WAYLAND=0)
endif()

# ============================================================
# Project include directories
# ============================================================
set(PROJECT_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/views
)

if(USE_WAYLAND)
    list(APPEND PROJECT_INCLUDE_DIRS ${WAYLAND_CLIENT_INCLUDE_DIRS})
endif()

# ============================================================
# Source files (auto-discover with CONFIGURE_DEPENDS)
# ============================================================
file(GLOB PROJECT_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/views/*.c
)
file(GLOB PROJECT_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/views/*.h
)

# ============================================================
# Target: lvgl_linux (shared library)
# ============================================================
add_library(lvgl_linux SHARED ${PROJECT_SOURCES})

set_target_properties(lvgl_linux PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "${PROJECT_HEADERS}"
    LINKER_LANGUAGE C
)

target_include_directories(lvgl_linux PRIVATE
    ${PROJECT_INCLUDE_DIRS}
    ${CROSS_COMPILE_INCLUDE_DIRS}
)

# ============================================================
# Target: lvglsim (executable)
# ============================================================
add_executable(lvglsim src/main.c)

target_include_directories(lvglsim PRIVATE
    ${PROJECT_INCLUDE_DIRS}
    ${CROSS_COMPILE_INCLUDE_DIRS}
)

# ============================================================
# Compile options (per-target, no inherited flags overwritten)
# ============================================================
if(USE_WAYLAND)
    set(PROJECT_COMPILE_OPTIONS -Wall -Wextra -Wpedantic -g)
else()
    set(PROJECT_COMPILE_OPTIONS -Wall -Wextra -Wpedantic -g -mfpu=neon)
endif()

target_compile_options(lvgl_linux PRIVATE ${PROJECT_COMPILE_OPTIONS})
target_compile_options(lvglsim PRIVATE ${PROJECT_COMPILE_OPTIONS})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(USE_WAYLAND)
        message(STATUS "Wayland mode: Native x86 build, Debug (-O0)")
    else()
        message(STATUS "ARM NEON SIMD optimization enabled (-mfpu=neon), Debug mode (-O0)")
    endif()
    target_compile_options(lvgl_linux PRIVATE -O0)
    target_compile_options(lvglsim PRIVATE -O0)
else()
    if(USE_WAYLAND)
        message(STATUS "Wayland mode: Native x86 build, Release (-O2)")
    else()
        message(STATUS "ARM NEON SIMD optimization enabled (-mfpu=neon), Release mode (-O2)")
    endif()
    target_compile_options(lvgl_linux PRIVATE -O2)
    target_compile_options(lvglsim PRIVATE -O2)
endif()

# ============================================================
# USE_WAYLAND and LVGL config overrides for application targets
# (lvgl already has PUBLIC defs; lvgl_linux doesn't link lvgl, needs its own)
# ============================================================
if(USE_WAYLAND)
    target_compile_definitions(lvgl_linux PRIVATE
        USE_WAYLAND=1
        LV_USE_WAYLAND=1
        LV_USE_LINUX_FBDEV=0
        LV_USE_EVDEV=0
        LV_USE_DRAW_SW_ASM=LV_DRAW_SW_ASM_NONE
    )
    target_compile_definitions(lvglsim PRIVATE USE_WAYLAND=1)
else()
    target_compile_definitions(lvgl_linux PRIVATE USE_WAYLAND=0)
    target_compile_definitions(lvglsim PRIVATE USE_WAYLAND=0)
endif()

# ============================================================
# Link libraries
# ============================================================
if(USE_WAYLAND)
    target_link_directories(lvglsim PRIVATE
        ${WAYLAND_CLIENT_LIBRARY_DIRS}
        ${WAYLAND_CURSOR_LIBRARY_DIRS}
        ${XKBCOMMON_LIBRARY_DIRS}
        ${ALSA_PC_LIBRARY_DIRS}
        ${FFMPEG_PC_LIBRARY_DIRS}
        ${ZLIB_PC_LIBRARY_DIRS}
        ${OPENSSL_PC_LIBRARY_DIRS}
    )

    target_link_libraries(lvglsim
        lvgl_linux
        lvgl
        ${WAYLAND_CLIENT_LIBRARIES}
        ${WAYLAND_CURSOR_LIBRARIES}
        ${XKBCOMMON_LIBRARIES}
        ${FFMPEG_LIBS}
        ${ALSA_PC_LIBRARIES}
        ${OPENSSL_PC_LIBRARIES}
        ${ZLIB_PC_LIBRARIES}
        m pthread dl
    )
else()
    target_link_directories(lvglsim PRIVATE
        ${EVDEV_LIB_DIR}
        ${OPENSSL_LIB_DIR}
        ${ZLIB_LIB_DIR}
        ${FFMPEG_LIB_DIR}
        ${ALSA_LIB_DIR}
    )

    target_link_libraries(lvglsim
        lvgl_linux
        lvgl
        -Wl,-Bstatic
        evdev
        -Wl,-Bdynamic
        ssl crypto
        ${FFMPEG_LIBS}
        asound
        z m pthread dl
    )
endif()

# ============================================================
# Post-build: strip debug symbols (Release only)
# ============================================================
if(NOT USE_WAYLAND AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET lvglsim POST_BUILD
        COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:lvglsim>
        COMMENT "Stripping debug symbols from lvglsim"
    )
elseif(USE_WAYLAND)
    message(STATUS "Wayland mode: skipping post-build strip")
else()
    message(STATUS "Debug mode: skipping post-build strip")
endif()

# ============================================================
# Installation rules
# ============================================================
install(TARGETS lvgl_linux lvglsim
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/v833_lv9_demos
)

# ============================================================
# Custom targets
# ============================================================
add_custom_target(run
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/lvglsim
    DEPENDS lvglsim
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running lvglsim"
)

add_custom_target(clean-all
    COMMAND ${CMAKE_BUILD_TOOL} clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeCache.txt
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
    COMMENT "Cleaning all build artifacts"
)

# ============================================================
# Pack target: bundle binary + all .so deps into pack/
#   Skipped in Wayland mode (native debug only)
# ============================================================
if(NOT USE_WAYLAND)
    set(PACK_DIR ${CMAKE_SOURCE_DIR}/pack)

    add_custom_target(pack
        DEPENDS lvglsim
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${PACK_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PACK_DIR}/lib
        # Main binary
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:lvglsim> ${PACK_DIR}/
        # Our own shared library: liblvgl_linux.so -> liblvgl_linux.so.1 -> liblvgl_linux.so.1.0.0
        COMMAND cp -a $<TARGET_FILE:lvgl_linux>        ${PACK_DIR}/lib/
        COMMAND cp -a $<TARGET_SONAME_FILE:lvgl_linux> ${PACK_DIR}/lib/
        COMMAND ${CMAKE_COMMAND} -E create_symlink liblvgl_linux.so.1 ${PACK_DIR}/lib/liblvgl_linux.so
        # Dependency .so files from sysroot
        COMMAND cp -a ${ZLIB_LIB_DIR}/libz.so*        ${PACK_DIR}/lib/
        COMMAND cp -a ${ALSA_LIB_DIR}/libasound.so*   ${PACK_DIR}/lib/
        COMMAND cp -a ${OPENSSL_LIB_DIR}/libssl.so*   ${PACK_DIR}/lib/
        COMMAND cp -a ${OPENSSL_LIB_DIR}/libcrypto.so* ${PACK_DIR}/lib/
        COMMAND cp -a ${FFMPEG_LIB_DIR}/libavcodec.so* ${PACK_DIR}/lib/
        COMMAND cp -a ${FFMPEG_LIB_DIR}/libavutil.so*  ${PACK_DIR}/lib/
        COMMAND cp -a ${FFMPEG_LIB_DIR}/libavformat.so* ${PACK_DIR}/lib/
        COMMAND cp -a ${FFMPEG_LIB_DIR}/libswscale.so* ${PACK_DIR}/lib/
        COMMAND cp -a ${FFMPEG_LIB_DIR}/libswresample.so* ${PACK_DIR}/lib/
        COMMAND cp -a ${FFMPEG_LIB_DIR}/libavdevice.so* ${PACK_DIR}/lib/
        COMMAND cp -a ${EVDEV_LIB_DIR}/libevdev.so*   ${PACK_DIR}/lib/
        # Strip debug from .so files (silently skip if already stripped)
        COMMAND ${CMAKE_STRIP} --strip-unneeded ${PACK_DIR}/lib/*.so.* 2>/dev/null || true
        # Launch wrapper
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/run.sh.in ${PACK_DIR}/run.sh
        COMMAND chmod +x ${PACK_DIR}/run.sh
        COMMENT "Packing deployment files into ${PACK_DIR}"
    )
endif()

# ============================================================
# Configuration summary
# ============================================================
message(STATUS "")
message(STATUS "==========================================")
message(STATUS "  V853s LV9 Demos Configuration Summary")
message(STATUS "==========================================")
message(STATUS "  Version:         ${PROJECT_VERSION}")
message(STATUS "  Build Type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Standard:      ${CMAKE_C_STANDARD}")
message(STATUS "  C++ Standard:    ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:        ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  Wayland Mode:    ${USE_WAYLAND}")
message(STATUS "  Install Prefix:  ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Sysroot:         ${SYSROOT}")
message(STATUS "==========================================")
message(STATUS "")
