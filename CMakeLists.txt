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

# Linker flags (LDFLAGS)
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

# Per-package paths
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

message(STATUS "Using sysroot: ${SYSROOT}")

# ============================================================
# Cross-compile include dirs (must be defined before LVGL subdirectory)
# ============================================================
set(CROSS_COMPILE_INCLUDE_DIRS
    ${ALSA_INCLUDE_DIR}
    ${FFMPEG_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIR}
)

# ============================================================
# LVGL configuration
# ============================================================
set(LV_BUILD_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h CACHE STRING "" FORCE)
set(LV_BUILD_SET_CONFIG_OPTS ON CACHE BOOL
    "create CMAKE variables from lv_conf_internal.h" FORCE)
add_subdirectory(lvgl)

# LVGL needs cross-compile includes for its optional modules (FFmpeg, etc.)
target_include_directories(lvgl PRIVATE ${CROSS_COMPILE_INCLUDE_DIRS})

# ============================================================
# Third-party UI library
# ============================================================
add_subdirectory(src/lib/lv_lib_100ask)

# ============================================================
# Lua static library
# ============================================================
set(LUA_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/lapi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/lfunc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/lstate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/lvm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/ltable.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/lstring.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/lauxlib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/lbaselib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/lstrlib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/liolib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/loslib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/lmathlib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/lutf8lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/linit.c
)

set(LUA_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/lua.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/lua.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/luaconf.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/lualib.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/lauxlib.h
)

add_library(lua STATIC ${LUA_SOURCES})

set_target_properties(lua PROPERTIES
    VERSION ${PROJECT_VERSION}
    PUBLIC_HEADER "${LUA_HEADERS}"
)

target_include_directories(lua PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua
)

# ============================================================
# Project include directories
# ============================================================
set(PROJECT_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/virsual_novel
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lv_lib_100ask
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua
)

# ============================================================
# Source files (auto-discover with CONFIGURE_DEPENDS)
# ============================================================
file(GLOB PROJECT_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/virsual_novel/*.c
)
file(GLOB PROJECT_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/virsual_novel/*.h
)

# ============================================================
# Target: lvgl_linux (static library)
# ============================================================
add_library(lvgl_linux STATIC ${PROJECT_SOURCES})

set_target_properties(lvgl_linux PROPERTIES
    VERSION ${PROJECT_VERSION}
    PUBLIC_HEADER "${PROJECT_HEADERS}"
    LINKER_LANGUAGE C
    COMPILE_DEFINITIONS "${LVGL_COMPILER_DEFINITIONS}"
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
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug mode (-O0)")
    target_compile_options(lvgl_linux PRIVATE -Wall -Wextra -Wpedantic -g -O0)
    target_compile_options(lvglsim PRIVATE -Wall -Wextra -Wpedantic -g -O0)
else()
    message(STATUS "Release mode (-O2)")
    target_compile_options(lvgl_linux PRIVATE -Wall -Wextra -Wpedantic -g -O2)
    target_compile_options(lvglsim PRIVATE -Wall -Wextra -Wpedantic -g -O2)
endif()

# ============================================================
# Link libraries
# ============================================================
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
    lv_lib_100ask
    lua
    -Wl,-Bstatic
    evdev
    -Wl,-Bdynamic
    ssl crypto
    -lavcodec -lavutil -lavformat -lswscale -lswresample -lavdevice
    asound
    z m pthread dl
)

# ============================================================
# Post-build: strip debug symbols (Release only)
# ============================================================
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET lvglsim POST_BUILD
        COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:lvglsim>
        COMMENT "Stripping debug symbols from lvglsim"
    )
else()
    message(STATUS "Debug mode: skipping post-build strip")
endif()

# ============================================================
# Installation rules
# ============================================================
install(TARGETS lvgl_linux lua lvglsim
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
# Configuration summary
# ============================================================
message(STATUS "")
message(STATUS "==========================================")
message(STATUS "  V833 LV9 Demos Configuration Summary")
message(STATUS "==========================================")
message(STATUS "  Version:         ${PROJECT_VERSION}")
message(STATUS "  Build Type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Standard:      ${CMAKE_C_STANDARD}")
message(STATUS "  C++ Standard:    ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:        ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  Install Prefix:  ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Sysroot:         ${SYSROOT}")
message(STATUS "==========================================")
message(STATUS "")
