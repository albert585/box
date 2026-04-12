# Install script for directory: /home/albert/Templates/v853_lv9_demos

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "0")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/home/albert/x-tools/arm-unknown-linux-musleabihf/bin/arm-unknown-linux-musleabihf-objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/home/albert/Templates/v853_lv9_demos/build_test/lib/liblvgl_linux.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/v833_lv9_demos" TYPE FILE FILES
    "/home/albert/Templates/v853_lv9_demos/src/lib/audio.h"
    "/home/albert/Templates/v853_lv9_demos/src/lib/audio_ctrl.h"
    "/home/albert/Templates/v853_lv9_demos/src/lib/button.h"
    "/home/albert/Templates/v853_lv9_demos/src/lib/container.h"
    "/home/albert/Templates/v853_lv9_demos/src/lib/events.h"
    "/home/albert/Templates/v853_lv9_demos/src/lib/eyesee_player.h"
    "/home/albert/Templates/v853_lv9_demos/src/lib/ff_player.h"
    "/home/albert/Templates/v853_lv9_demos/src/lib/file_manager.h"
    "/home/albert/Templates/v853_lv9_demos/src/lib/player.h"
    "/home/albert/Templates/v853_lv9_demos/src/lib/settings.h"
    "/home/albert/Templates/v853_lv9_demos/src/lib/views/lv_text_clock.h"
    "/home/albert/Templates/v853_lv9_demos/src/lib/virsual_novel/cJSON.h"
    "/home/albert/Templates/v853_lv9_demos/src/lib/virsual_novel/data_parser.h"
    "/home/albert/Templates/v853_lv9_demos/src/lib/virsual_novel/resource_manager.h"
    "/home/albert/Templates/v853_lv9_demos/src/lib/virsual_novel/simple_json.h"
    "/home/albert/Templates/v853_lv9_demos/src/lib/virsual_novel/visual_novel_engine.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/home/albert/Templates/v853_lv9_demos/build_test/lib/liblua.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/v833_lv9_demos" TYPE FILE FILES
    "/home/albert/Templates/v853_lv9_demos/src/lib/lua/lua.h"
    "/home/albert/Templates/v853_lv9_demos/src/lib/lua/lua.hpp"
    "/home/albert/Templates/v853_lv9_demos/src/lib/lua/luaconf.h"
    "/home/albert/Templates/v853_lv9_demos/src/lib/lua/lualib.h"
    "/home/albert/Templates/v853_lv9_demos/src/lib/lua/lauxlib.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/lvglsim" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/lvglsim")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/lvglsim"
         RPATH "")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/home/albert/Templates/v853_lv9_demos/build_test/bin/lvglsim")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/lvglsim" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/lvglsim")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/home/albert/x-tools/arm-unknown-linux-musleabihf/bin/arm-unknown-linux-musleabihf-strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/lvglsim")
    endif()
  endif()
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/home/albert/Templates/v853_lv9_demos/build_test/lvgl/cmake_install.cmake")
  include("/home/albert/Templates/v853_lv9_demos/build_test/src/lib/lv_lib_100ask/cmake_install.cmake")

endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/albert/Templates/v853_lv9_demos/build_test/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/albert/Templates/v853_lv9_demos/build_test/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
