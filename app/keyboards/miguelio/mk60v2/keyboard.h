#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "bmk.h"

#define MATRIX_COLS 8
#define MATRIX_ROWS 9
#define LAYERS 2
#define ENCODERS 1
#define POWER_EXT true
#define RGB true
#define RGB_EFFECTS false
#define BATTERY true
#define SLEEP_TIMEOUT 3
#define DEBOUNCE_PRESS 5
#define DEBOUNCE_RELEASE 15


extern const uint16_t layers[LAYERS][MATRIX_COLS * MATRIX_ROWS + ENCODERS * ENCODER_PINS];

#endif