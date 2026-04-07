#ifndef MATRIX_H
#define MATRIX_H

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/drivers/gpio.h>

#define MATRIX_COLS 8
#define MATRIX_ROWS 9

#define DEBOUNCE_PRESS 5
#define DEBOUNCE_RELEASE 20

extern const struct gpio_dt_spec cols[MATRIX_COLS];
extern const struct gpio_dt_spec rows[MATRIX_ROWS];

#endif