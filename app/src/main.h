#ifndef MAIN_H
#define MAIN_H

#include "keyboard.h"
#include "battery/battery.h"

// Static variables
#define SEND_THREAD_STACK_SIZE 1024
#define SEND_THREAD_PRIORITY 7
#define SEND_THREAD_CACHE_SIZE 16
#define CYCLE_BASE_DELAY 1000

// Config parameters
#ifndef DEBOUNCE_PRESS
#define DEBOUNCE_PRESS 5
#endif
#ifndef DEBOUNCE_RELEASE
#define DEBOUNCE_RELEASE 15
#endif
#ifndef DEBOUNCE_ENCODER
#define DEBOUNCE_ENCODER 5
#endif
#ifndef CYCLE_DELAY
#define CYCLE_DELAY CYCLE_BASE_DELAY
#endif
#ifndef SLEEP_TIMEOUT
#define SLEEP_TIMEOUT 5
#endif
#ifndef SLEEP_TIMEOUT_ADV
#define SLEEP_TIMEOUT_ADV 300
#endif
#define SLEEP_TIMEOUT_MS (SLEEP_TIMEOUT * 1000)
#define SLEEP_TIMEOUT_ADV_MS (SLEEP_TIMEOUT_ADV * 1000)
#ifndef TAP_HOLD_PRESS_DELAY
#define TAP_HOLD_PRESS_DELAY 175
#endif
#ifndef TAP_HOLD_RELEASE_DELAY
#define TAP_HOLD_RELEASE_DELAY 150
#endif
#ifndef TAP_HOLD_SIZE_ARRAY
#define TAP_HOLD_SIZE_ARRAY 3
#endif
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_DBG
#endif
#ifndef ENCODERS
#define ENCODERS 0
#endif
#ifndef USB
#define USB true
#endif
#ifndef USB_CHECK_TIMEOUT
#define USB_CHECK_TIMEOUT 1000
#endif

#ifndef RGB
#define RGB false
#endif
#ifndef RGB_COLOR_JUMP
#define RGB_COLOR_JUMP 5
#endif
#ifndef RGB_LIGHT_JUMP
#define RGB_LIGHT_JUMP 32
#endif
#ifndef RGB_SATURATION_JUMP
#define RGB_SATURATION_JUMP 32
#endif
#ifndef RGB_ON_STARTUP
#define RGB_ON_STARTUP true
#endif
#ifndef RGB_ON_STARTUP
#define RGB_ON_STARTUP true
#endif
#ifndef RGB_EFFECTS
#define RGB_EFFECTS RGB
#endif
#ifndef RGB_EFF_RAINBOW
#define RGB_EFF_RAINBOW RGB_EFFECTS
#endif
#ifndef RGB_EFF_COLORS
#define RGB_EFF_COLORS RGB_EFFECTS
#endif
#ifndef RGB_EFF_KITT
#define RGB_EFF_KITT RGB_EFFECTS
#endif

#ifndef POWER_EXT
#define POWER_EXT false
#endif
#ifndef POWER_EXT_RGB_LINKED
#define POWER_EXT_RGB_LINKED true
#endif
#ifndef POWER_EXT_RGB_DELAY
#define POWER_EXT_RGB_DELAY 5
#endif
#ifndef POWER_EXT_ON
#define POWER_EXT_ON true
#endif

#ifndef BATTERY
#define BATTERY true
#endif

#ifndef LOGS
#define LOGS false
#endif

struct key
{
    uint16_t kc[LAYERS];
    bool pressed;
    bool tapped;
    bool held;
    uint8_t debounce_count;
    uint8_t delay_press_count;
    uint8_t delay_release_count;
};

struct encoder_key
{
    uint16_t left_kc[LAYERS];
    uint16_t right_kc[LAYERS];
    uint8_t last_value;
    uint8_t debounce_count;
    int direction;
};

typedef enum
{
    BMK_KEYBOARD = BMK_HID_REPORT_ID_KEYBOARD,
    BMK_CONSUMER = BMK_HID_REPORT_ID_CONSUMER
} bmk_report_type_t;

typedef struct
{
    bmk_report_type_t type;
    uint8_t report[9];
    uint8_t report_consumer[7];
} thread_report_t;

typedef struct
{
    uint8_t idx;
    uint8_t layer;
} held_mod_key_t;

struct timeout_tapped_keys
{
    struct k_work_delayable dwork;
    uint8_t key_idx;
};

#endif