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

struct key {
    uint16_t kc[LAYERS];
    bool pressed;
    uint8_t debounce_count;
};

struct encoder_key {
    uint16_t kc[LAYERS];
    uint8_t last_value;
    uint8_t debounce_count;
    uint16_t step_count;
};

#endif