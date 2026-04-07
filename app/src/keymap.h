#ifndef KEYMAP_H
#define KEYMAP_H

#include "matrix.h"

#define LAYERS 2

#define HID_KEY_NUBS 0x64
#define HID_KEY_NONE 0

struct key {
    uint8_t kc[LAYERS];
    bool pressed;
    uint8_t debounce_count;
};

extern const uint8_t layers[LAYERS][MATRIX_COLS * MATRIX_ROWS];

#endif
