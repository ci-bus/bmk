#ifndef MAIN_H
#define MAIN_H

#include "keyboard.h"

#ifndef DEBOUNCE_PRESS
#define DEBOUNCE_PRESS 5
#endif

#ifndef DEBOUNCE_RELEASE
#define DEBOUNCE_RELEASE 5
#endif

#ifndef DEBOUNCE_ENCODER
#define DEBOUNCE_ENCODER 5
#endif

#define SEND_THREAD_STACK_SIZE 1024
#define SEND_THREAD_PRIORITY 1
#ifndef SEND_THREAD_CACHE_SIZE
#define SEND_THREAD_CACHE_SIZE 5
#endif

struct key {
    uint16_t kc[LAYERS];
    bool pressed;
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

struct thread_report {
    bool ready;
    bmk_report_type_t type;
    uint8_t report[9];
    uint8_t report_consumer[7];
};

#endif