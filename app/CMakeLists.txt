cmake_minimum_required(VERSION 3.20.0)

set(NO_BUILD_TYPE_WARNING ON)
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

set(RGB_OVERLAY "keyboards/${KEYBOARD}/rgb.overlay")
set(RGB_CONF    "boards/arm/common/rgb.conf")

if(EXISTS ${RGB_OVERLAY})
  message(STATUS "RGB habilitado para ${KEYBOARD}")
  list(APPEND EXTRA_CONF_FILE ${RGB_CONF})
  set(DTC_OVERLAY_FILE 
    "keyboards/${KEYBOARD}/rgb.overlay"
  )
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(keyboard-firmware)

if(NOT DEFINED KEYBOARD)
  message(FATAL_ERROR "KEYBOARD not defined. Example: -DKEYBOARD=miguelio/mk60v1")
endif()

target_include_directories(app PRIVATE src)
target_include_directories(app PRIVATE keyboards/${KEYBOARD})

target_sources(app PRIVATE
  keyboards/${KEYBOARD}/keyboard.c
  keyboards/${KEYBOARD}/keymap.c
  src/main.c
)
