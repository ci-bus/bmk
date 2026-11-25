#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "bmk.h"

#define MATRIX_COLS 15
#define MATRIX_ROWS 6
#define LAYERS 2
#define ENCODERS 1
#define POWER_EXT true
#define RGB true
#define RGB_PORT 1
#define RGB_PIN 6
#define RGB_LEDS 6
#define RGB_EFFECT_ON_STARTUP 2
#define BATTERY true

extern const uint16_t layers[LAYERS][MATRIX_COLS * MATRIX_ROWS + ENCODERS * ENCODER_PINS];

#endif
