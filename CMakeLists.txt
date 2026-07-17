cmake_minimum_required(VERSION 3.12)

# Project configuration
project(v833_lv9_demos C)
set(PROJECT_VERSION "1.0.0")

# CMake policies
cmake_policy(SET CMP0079 NEW)

# Build configuration (set after ARCH is known)
# C standard: gnu99 for wayland (GCC 16 stricter), c99 for cross-compile

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ============================================================
# Target chip / build mode
# ============================================================
set(ARCH "wayland" CACHE STRING "Target architecture: v833, v853, wayland")

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ============================================================
# Architecture-dependent dependency setup
# ============================================================
if(ARCH STREQUAL "wayland")
    # Native Linux: use system packages via pkg-config
    find_package(PkgConfig REQUIRED)

    # LVGL Wayland driver deps
    pkg_check_modules(WAYLAND REQUIRED wayland-client wayland-cursor xkbcommon)

    # App dependencies
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
        libavcodec libavformat libavutil libswscale libswresample)
    pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)

    set(DEP_INCLUDE_DIRS
        ${WAYLAND_INCLUDE_DIRS}
    )
    set(DEP_LIBS
        ${WAYLAND_LIBRARIES}
        PkgConfig::FFMPEG
        PkgConfig::ALSA
        z m pthread dl
    )

    message(STATUS "Build mode: wayland (native)")
else()
    # Cross-compile: v833 / v853
    set(CMAKE_C_STANDARD 99)
    set(CMAKE_C_STANDARD_REQUIRED ON)

    # Linker flags
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")

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

    set(DEP_INCLUDE_DIRS
        ${ALSA_INCLUDE_DIR}
        ${FFMPEG_INCLUDE_DIR}
        ${ZLIB_INCLUDE_DIR}
    )

    set(FFMPEG_LIBS -lavcodec -lavutil -lavformat -lswscale -lswresample -lavdevice)

    set(DEP_LIBS
        ${FFMPEG_LIBS}
        asound
        ssl crypto
        z m pthread dl
    )

    message(STATUS "Build mode: ${ARCH} (cross-compile)")
    message(STATUS "  Sysroot: ${SYSROOT}")
endif()

# ============================================================
# LVGL configuration (built as shared library liblvgl_linux.so)
# ============================================================
set(BUILD_SHARED_LIBS ON)
set(LV_BUILD_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/config/lv_conf_${ARCH}.h CACHE STRING "" FORCE)
set(LV_BUILD_SET_CONFIG_OPTS ON CACHE BOOL
    "create CMAKE variables from lv_conf_internal.h" FORCE)
add_subdirectory(lvgl)

set_target_properties(lvgl PROPERTIES OUTPUT_NAME lvgl_linux)

target_include_directories(lvgl PRIVATE ${DEP_INCLUDE_DIRS})

if(ARCH STREQUAL "wayland")
    target_include_directories(lvgl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/config)
    target_sources(lvgl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/config/wayland_xdg_shell.c)
endif()

# ============================================================
# Project include directories
# ============================================================
set(PROJECT_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/views
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/battery
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/games
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/src/arch
)

# ============================================================
# Source files (auto-discover with CONFIGURE_DEPENDS)
# ============================================================
file(GLOB BOX_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/views/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/audio/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/battery/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/utils/*.c
)

file(GLOB ARCH_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/${ARCH}/*.c
)
if(NOT ARCH_SOURCES)
    message(FATAL_ERROR "No source files found for architecture '${ARCH}' under src/arch/${ARCH}/")
endif()

# ============================================================
# Target: box (executable) — all src/ code goes here
# ============================================================
add_executable(box src/main.c ${BOX_SOURCES} ${ARCH_SOURCES})

target_include_directories(box PRIVATE
    ${PROJECT_INCLUDE_DIRS}
    ${DEP_INCLUDE_DIRS}
)

# ============================================================
# Compile options (per-target, no inherited flags overwritten)
# ============================================================
if(ARCH STREQUAL "wayland")
    set(PROJECT_COMPILE_OPTIONS -std=gnu99 -Wall -Wextra -g
        -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types
        -Wno-declaration-missing-parameter-type)
else()
    set(PROJECT_COMPILE_OPTIONS -Wall -Wextra -Wpedantic -g)
endif()

target_compile_options(box PRIVATE ${PROJECT_COMPILE_OPTIONS})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug mode: -O0")
    target_compile_options(box PRIVATE -O0)
else()
    message(STATUS "Release mode: -O2")
    target_compile_options(box PRIVATE -O2)
endif()

# ============================================================
# Link libraries
# ============================================================
if(ARCH STREQUAL "wayland")
    target_link_libraries(box lvgl ${DEP_LIBS})
else()
    target_link_directories(box PRIVATE
        ${EVDEV_LIB_DIR}
        ${OPENSSL_LIB_DIR}
        ${ZLIB_LIB_DIR}
        ${FFMPEG_LIB_DIR}
        ${ALSA_LIB_DIR}
    )

    find_library(EVDEV_LIB evdev PATHS ${EVDEV_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(box lvgl ${EVDEV_LIB} ${DEP_LIBS})
endif()

# ============================================================
# Post-build: strip debug symbols (Release only)
# ============================================================
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET box POST_BUILD
        COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:box>
        COMMENT "Stripping debug symbols from box"
    )
else()
    message(STATUS "Debug mode: skipping post-build strip")
endif()

# ============================================================
# Installation rules
# ============================================================
install(TARGETS lvgl box
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# ============================================================
# pack target: copy built artifacts to pack/ for deployment
# ============================================================
if(NOT ARCH STREQUAL "wayland")
    set(PROJECT_EXECUTABLE "box")
    configure_file(${CMAKE_SOURCE_DIR}/pack/run.sh.in ${CMAKE_SOURCE_DIR}/pack/run.sh @ONLY)

    add_custom_target(pack
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:box> ${CMAKE_SOURCE_DIR}/pack/box
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:lvgl> ${CMAKE_SOURCE_DIR}/pack/lib/
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_LINKER_FILE:lvgl> ${CMAKE_SOURCE_DIR}/pack/lib/
        COMMENT "Packing box and liblvgl_linux.so to pack/"
        DEPENDS box lvgl
    )
endif()

# ============================================================
# Configuration summary
# ============================================================
message(STATUS "")
message(STATUS "==========================================")
message(STATUS "  V833 LV9 Demos Configuration Summary")
message(STATUS "==========================================")
message(STATUS "  Version:         ${PROJECT_VERSION}")
message(STATUS "  Build Type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Standard:      ${CMAKE_C_STANDARD}")
message(STATUS "  Compiler:        ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  Arch:            ${ARCH}")
message(STATUS "  Install Prefix:  ${CMAKE_INSTALL_PREFIX}")
if(ARCH STREQUAL "wayland")
    message(STATUS "  Build mode:      native (system libs)")
else()
    message(STATUS "  Sysroot:         ${SYSROOT}")
endif()
message(STATUS "==========================================")
message(STATUS "")
