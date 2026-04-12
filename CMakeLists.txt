cmake_minimum_required(VERSION 3.10)

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

# Set compiler flags based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Debug mode: no optimization, keep debug symbols
    set(CMAKE_C_FLAGS "-Wall -Wextra -Wpedantic -g -O0 -mfpu=neon" CACHE STRING "C compiler flags" FORCE)
    set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -g -O0 -mfpu=neon" CACHE STRING "C++ compiler flags" FORCE)
else()
    # Release mode: optimization enabled
    set(CMAKE_C_FLAGS "-Wall -Wextra -Wpedantic -g -O2 -mfpu=neon" CACHE STRING "C compiler flags" FORCE)
    set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -g -O2 -mfpu=neon" CACHE STRING "C++ compiler flags" FORCE)
endif()

# ARM NEON SIMD optimization support
message(STATUS "ARM NEON SIMD optimization enabled (-mfpu=neon)")

# Eyesee-MPP hardware decoder support
option(USE_EYEESEE_MPP "Enable Eyesee-MPP hardware video decoder support" ON)
if(USE_EYEESEE_MPP)
    message(STATUS "Eyesee-MPP hardware decoder support enabled")
    add_definitions(-DUSE_EYEESEE_MPP=1)
else()
    message(STATUS "Eyesee-MPP hardware decoder support disabled")
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Linker flags (LDFLAGS)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")

# Cross-compilation toolchain paths (for ARM musl toolchain)
set(CROSS_COMPILE_INCLUDE_DIRS /srv/alsa/include /srv/ffmpeg/include /srv/zlib-armv7/include)

# FFmpeg version selection
set(FFMPEG_VERSION "6" CACHE STRING "FFmpeg version to use (4 or 6)")
set_property(CACHE FFMPEG_VERSION PROPERTY STRINGS "4" "6")

if(FFMPEG_VERSION STREQUAL "4")
    message(STATUS "Using FFmpeg 4.x (from /srv/ffmpeg-ssd)")
    set(FFMPEG_INCLUDE_DIR /srv/ffmpeg-ssd/include)
    set(FFMPEG_LIB_DIR /srv/ffmpeg-ssd/lib)
    set(FFMPEG_LIBS -lavcodec -lavutil -lavformat -lswscale -lswresample)
    # FFmpeg 4.x does not have avdevice, enforce ALSA PCM mode
    add_definitions(-DFFMPEG_VERSION_4)
    add_definitions(-DLV_FFMPEG_USE_AVDEVICE=0)
elseif(FFMPEG_VERSION STREQUAL "6")
    message(STATUS "Using FFmpeg 6.x (from /srv/ffmpeg)")
    set(FFMPEG_INCLUDE_DIR /srv/ffmpeg/include)
    set(FFMPEG_LIB_DIR /srv/ffmpeg/lib)
    set(FFMPEG_LIBS -lavcodec -lavutil -lavformat -lswscale -lswresample -lavdevice)
else()
    message(FATAL_ERROR "Invalid FFMPEG_VERSION: ${FFMPEG_VERSION}. Must be '4' or '6'")
endif()

# Update CROSS_COMPILE_INCLUDE_DIRS with selected FFmpeg
set(CROSS_COMPILE_INCLUDE_DIRS /srv/alsa/include ${FFMPEG_INCLUDE_DIR} /srv/zlib-armv7/include)

# Add cross-compile include directories
foreach(DIR ${CROSS_COMPILE_INCLUDE_DIRS})
    include_directories(${DIR})
endforeach()

# LVGL configuration
set(LV_BUILD_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h CACHE STRING "" FORCE)
set(LV_BUILD_SET_CONFIG_OPTS ON CACHE BOOL
    "create CMAKE variables from lv_conf_internal.h" FORCE)

# Add LVGL subdirectory
add_subdirectory(lvgl)

# Add lv_lib_100ask subdirectory
add_subdirectory(src/lib/lv_lib_100ask)

# Lua library source files
file(GLOB LUA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/*.c)

# Lua library header files
set(LUA_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/lua.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/lua.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/luaconf.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/lualib.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/lauxlib.h
)

# Create lua static library
add_library(lua STATIC ${LUA_SOURCES})

# Set library properties
set_target_properties(lua PROPERTIES
    VERSION ${PROJECT_VERSION}
    PUBLIC_HEADER "${LUA_HEADERS}"
)

# Add include directories for lua
target_include_directories(lua PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua
)

# Eyesee-MPP configuration
set(EYEESEE_MPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/eyesee-mpp CACHE STRING "Eyesee-MPP library directory")
set(EYEESEE_MPP_LIB_DIR ${EYEESEE_MPP_DIR}/lib/eyesee-mpp CACHE STRING "Eyesee-MPP .so directory")
set(EYEESEE_MPP_SYSROOT_DIR ${EYEESEE_MPP_DIR}/lib CACHE STRING "Eyesee-MPP sysroot library directory")
set(EYEESEE_MPP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include CACHE STRING "Eyesee-MPP include directory")

# Project include directories
set(PROJECT_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/virsual_novel
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lv_lib_100ask/
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/lua/
    ${EYEESEE_MPP_INCLUDE_DIR}
)

# Project source files (auto-discover all .c files in src/ except main.c)
file(GLOB PROJECT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/*.c ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/virsual_novel/*.c ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/views/*.c)

# Project header files (auto-discover all .h files in src/)
file(GLOB PROJECT_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/*.h ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/virsual_novel/*.h ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/views/*.h)

# Create lvgl_linux static library
add_library(lvgl_linux STATIC ${PROJECT_SOURCES})

# Set library properties
set_target_properties(lvgl_linux PROPERTIES
    VERSION ${PROJECT_VERSION}
    PUBLIC_HEADER "${PROJECT_HEADERS}"
)

# Add include directories for lvgl_linux
target_include_directories(lvgl_linux PUBLIC
    ${PROJECT_INCLUDE_DIRS}
)

# Set compile definitions
set_target_properties(lvgl_linux PROPERTIES
    COMPILE_DEFINITIONS "${LVGL_COMPILER_DEFINITIONS}"
)

# Create main executable
add_executable(lvglsim src/main.c)

# Strip debug symbols from lvglsim after building
add_custom_command(TARGET lvglsim POST_BUILD
    COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:lvglsim>
    COMMENT "Stripping debug symbols from lvglsim"
)

# Set executable properties
set_target_properties(lvgl_linux PROPERTIES
    LINKER_LANGUAGE C
)

# Add include directories for executable
target_include_directories(lvglsim PRIVATE
    ${PROJECT_INCLUDE_DIRS}
    ${CROSS_COMPILE_INCLUDE_DIRS}
)

# Eyesee-MPP libraries (minimal set for video decoding)
# Core decoder .so files are in eyesee-mpp/lib/eyesee-mpp/
set(EYEESEE_MPP_LIBS
    -L${EYEESEE_MPP_LIB_DIR}
    -Wl,--start-group
    -lvdecoder
    -lMemAdapter
    -lcdc_base
    -lVE
    -lvideoengine
    -lion
    -llog
    -Wl,--end-group
)

# Add library directories for linking
# Link libraries using traditional linker flags
target_link_libraries(lvglsim lvgl_linux lvgl lv_lib_100ask lua -L/srv/evdev/lib -L/srv/openssl/lib -L/srv/zlib/lib -L${FFMPEG_LIB_DIR} -L/srv/alsa/lib -L${EYEESEE_MPP_SYSROOT_DIR} -Wl,-rpath-link,${EYEESEE_MPP_SYSROOT_DIR} -Wl,-Bstatic -levdev -Wl,-Bdynamic -lssl -lcrypto ${FFMPEG_LIBS} -lasound -lz -lm -lpthread -ldl ${EYEESEE_MPP_LIBS})

# Installation rules
install(TARGETS lvgl_linux lua lvglsim
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/v833_lv9_demos
)

# Custom targets
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

# Print configuration summary
message(STATUS "")
message(STATUS "==========================================")
message(STATUS "  V833 LV9 Demos Configuration Summary")
message(STATUS "==========================================")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Standard:     ${CMAKE_C_STANDARD}")
message(STATUS "  C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:       ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "==========================================")
message(STATUS "")
