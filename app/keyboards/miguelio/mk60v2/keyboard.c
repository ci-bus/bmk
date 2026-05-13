#include "keyboard.h"

#if defined(CONFIG_SOC_NRF52833_QIAA)
#define COL4_PIN 5
#else
#define COL4_PIN 13
#endif

const struct gpio_dt_spec cols[MATRIX_COLS] = {
    {.port = GPIO0, .pin = 28, .dt_flags = GPIO_ACTIVE_HIGH},       /* Col 0: P0.28 */
    {.port = GPIO0, .pin = 15, .dt_flags = GPIO_ACTIVE_HIGH},       /* Col 1: P0.15 */
    {.port = GPIO1, .pin = 0, .dt_flags = GPIO_ACTIVE_HIGH},        /* Col 2: P1.00 */
    {.port = GPIO0, .pin = 10, .dt_flags = GPIO_ACTIVE_HIGH},       /* Col 3: P0.10 */
    {.port = GPIO1, .pin = COL4_PIN, .dt_flags = GPIO_ACTIVE_HIGH}, /* Col 4: P1.13 */
    {.port = GPIO0, .pin = 3, .dt_flags = GPIO_ACTIVE_HIGH},        /* Col 5: P0.03 */
    {.port = GPIO0, .pin = 2, .dt_flags = GPIO_ACTIVE_HIGH},        /* Col 6: P0.02 */
    {.port = GPIO0, .pin = 29, .dt_flags = GPIO_ACTIVE_HIGH},       /* Col 7: P0.29 */
};

const struct gpio_dt_spec rows[MATRIX_ROWS] = {
    {.port = GPIO0, .pin = 13, .dt_flags = GPIO_ACTIVE_HIGH}, /* Row 0: P0.13 */
    {.port = GPIO0, .pin = 24, .dt_flags = GPIO_ACTIVE_HIGH}, /* Row 1: P0.24 */
    {.port = GPIO0, .pin = 9, .dt_flags = GPIO_ACTIVE_HIGH},  /* Row 2: P0.09 */
    {.port = GPIO0, .pin = 8, .dt_flags = GPIO_ACTIVE_HIGH},  /* Row 3: P0.08 */
    {.port = GPIO0, .pin = 12, .dt_flags = GPIO_ACTIVE_HIGH}, /* Row 4: P0.12 */
    {.port = GPIO0, .pin = 7, .dt_flags = GPIO_ACTIVE_HIGH},  /* Row 5: P0.07 */
    {.port = GPIO1, .pin = 2, .dt_flags = GPIO_ACTIVE_HIGH},  /* Row 6: P1.02 */
    {.port = GPIO1, .pin = 4, .dt_flags = GPIO_ACTIVE_HIGH},  /* Row 7: P1.04 */
    {.port = GPIO0, .pin = 17, .dt_flags = GPIO_ACTIVE_HIGH}, /* Row 8: P0.17 */
};

const struct gpio_dt_spec encoders[ENCODERS * ENCODER_PINS] = {
    {.port = GPIO0, .pin = 6, .dt_flags = GPIO_ACTIVE_LOW}, /* Encoder 0 left: P0.06 */
    {.port = GPIO0, .pin = 5, .dt_flags = GPIO_ACTIVE_LOW}, /* Encoder 0 right: P0.05 */
};

const struct gpio_dt_spec power_ext = {
    .port = GPIO1, .pin = 9, .dt_flags = GPIO_ACTIVE_HIGH /* External power to rgb underglow */
};