# Usage:
# cmake -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_setup.cmake -B build -S .
# make  -C build -j

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(TOOLCHAIN_PREFIX "/usr/x-tools/arm-unknown-linux-musleabihf" CACHE PATH "Cross-compile toolchain prefix")

set(CMAKE_C_COMPILER   "${TOOLCHAIN_PREFIX}/bin/arm-unknown-linux-musleabihf-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PREFIX}/bin/arm-unknown-linux-musleabihf-g++")
set(CMAKE_STRIP        "${TOOLCHAIN_PREFIX}/bin/arm-unknown-linux-musleabihf-strip")

set(CMAKE_C_FLAGS_INIT "-mfpu=neon")
set(CMAKE_CXX_FLAGS_INIT "-mfpu=neon")

# Standard cross-compilation settings: only search sysroot, never host
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# If necessary, set STAGING_DIR
# if not work, please try(in shell command): export STAGING_DIR=/home/ubuntu/Your_SDK/out/xxx/openwrt/staging_dir/target
#set(ENV{STAGING_DIR} "/home/ubuntu/Your_SDK/out/xxx/openwrt/staging_dir/target")
