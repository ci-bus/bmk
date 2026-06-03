cmake_minimum_required(VERSION 3.20.0)

set(NO_BUILD_TYPE_WARNING ON)
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

set(KBD_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/keyboards/${KEYBOARD}/keyboard.h")

if(EXISTS ${KBD_HEADER})
  file(READ ${KBD_HEADER} _kbd_h)

  if(_kbd_h MATCHES "#define[ \t]+RGB[ \t]+true")
    string(REGEX MATCH "#define[ \t]+RGB_PORT[ \t]+([0-9]+)" _ "${_kbd_h}")
    set(RGB_PORT ${CMAKE_MATCH_1})
    string(REGEX MATCH "#define[ \t]+RGB_PIN[ \t]+([0-9]+)"  _ "${_kbd_h}")
    set(RGB_PIN  ${CMAKE_MATCH_1})
    string(REGEX MATCH "#define[ \t]+RGB_LEDS[ \t]+([0-9]+)" _ "${_kbd_h}")
    set(RGB_LEDS ${CMAKE_MATCH_1})

    if(RGB_PORT STREQUAL "" OR RGB_PIN STREQUAL "" OR RGB_LEDS STREQUAL "")
      message(FATAL_ERROR "RGB enabled for ${KEYBOARD} but RGB_PORT/RGB_PIN/RGB_LEDS not defined in keyboard.h")
    endif()

    message(STATUS "RGB enabled for ${KEYBOARD}: port=${RGB_PORT} pin=${RGB_PIN} leds=${RGB_LEDS}")
    list(APPEND EXTRA_CONF_FILE "boards/arm/common/rgb.conf")
    list(APPEND DTC_OVERLAY_FILE "boards/arm/common/rgb.overlay")
    list(APPEND DTS_EXTRA_CPPFLAGS
      "-DRGB_PORT=${RGB_PORT}"
      "-DRGB_PIN=${RGB_PIN}"
      "-DRGB_LEDS=${RGB_LEDS}")
  endif()

  if(_kbd_h MATCHES "#define[ \t]+LOGS[ \t]+true")
    list(APPEND EXTRA_CONF_FILE "boards/arm/common/logs.conf")
    list(APPEND DTC_OVERLAY_FILE "boards/arm/common/logs.overlay")
  endif()
endif()

list(APPEND DTC_OVERLAY_FILE "boards/arm/common/battery.overlay")

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
  src/battery/battery.c
  src/main.c
)
