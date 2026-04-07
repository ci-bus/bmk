cmake_minimum_required(VERSION 3.20.0)

set(NO_BUILD_TYPE_WARNING ON)
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(keyboard-firmware)

if(NOT DEFINED KEYBOARD)
  message(FATAL_ERROR "KEYBOARD not defined. Example: -DKEYBOARD=miguelio/mk60v1")
endif()

target_include_directories(app PRIVATE src)

target_sources(app PRIVATE
  keyboards/${KEYBOARD}/matrix.c
  keyboards/${KEYBOARD}/keymap.c
  src/main.c
)
