cmake_minimum_required(VERSION 3.20.0)

set(NO_BUILD_TYPE_WARNING ON)
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(keyboard-firmware)

target_sources(app PRIVATE
  src/main.c
  src/keymap.c
  src/matrix.c
)
