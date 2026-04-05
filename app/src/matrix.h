#ifndef MATRIX_H
#define MATRIX_H

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/devicetree.h>

#define MATRIX_NODE DT_PATH(matrix)
#define MATRIX_COLS DT_PROP_LEN(MATRIX_NODE, col_gpios)
#define MATRIX_ROWS DT_PROP_LEN(MATRIX_NODE, row_gpios)
#define DEBOUNCE_PRESS 5
#define DEBOUNCE_RELEASE 20

#define HID_KEY_NUBS 0x64

struct key {
    uint8_t kc;
    bool pressed;
    uint8_t debounce_count;
};

#endif