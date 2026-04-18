#include "keyboard.h"

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
#define DEBOUNCE_RELEASE 5
#endif
#ifndef DEBOUNCE_ENCODER
#define DEBOUNCE_ENCODER 5
#endif
#ifndef CYCLE_DELAY
#define CYCLE_DELAY CYCLE_BASE_DELAY
#endif
#ifndef SLEEP_TIMEOUT
#define SLEEP_TIMEOUT 5000
#endif
#ifndef TAP_HOLD_DELAY
#define TAP_HOLD_DELAY 200
#endif
#ifndef TAP_HOLD_SIZE_ARRAY
#define TAP_HOLD_SIZE_ARRAY 3
#endif

typedef enum {
    RELEASED = 0,
    PRESSED = 1,
    HELD = 2,
} bmk_key_status_t;

struct key {
    uint16_t kc[LAYERS];
    bmk_key_status_t status;
    uint8_t debounce_count;
};

struct encoder_key {
    uint16_t left_kc[LAYERS];
    uint16_t right_kc[LAYERS];
    uint8_t last_value;
    uint8_t debounce_count;
    int direction;
};

typedef enum {
    BMK_KEYBOARD = BMK_HID_REPORT_ID_KEYBOARD,
    BMK_CONSUMER = BMK_HID_REPORT_ID_CONSUMER
} bmk_report_type_t;

typedef struct {
    bmk_report_type_t type;
    uint8_t report[9];
    uint8_t report_consumer[7];
} thread_report_t;

typedef struct {
    uint8_t idx;
    uint8_t layer;
} held_mod_key_t;
