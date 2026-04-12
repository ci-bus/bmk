#include "keyboard.h"

const struct gpio_dt_spec cols[MATRIX_COLS] = {
    {.port = GPIO0, .pin = 28, .dt_flags = GPIO_ACTIVE_HIGH}, /* Col 0: P0.28 */
    {.port = GPIO0, .pin = 22, .dt_flags = GPIO_ACTIVE_HIGH}, /* Col 1: P0.22 */
    {.port = GPIO0, .pin = 31, .dt_flags = GPIO_ACTIVE_HIGH}, /* Col 2: P0.31 */
    {.port = GPIO0, .pin = 29, .dt_flags = GPIO_ACTIVE_HIGH}, /* Col 3: P0.29 */
    {.port = GPIO0, .pin = 2, .dt_flags = GPIO_ACTIVE_HIGH},  /* Col 4: P0.02 */
    {.port = GPIO1, .pin = 13, .dt_flags = GPIO_ACTIVE_HIGH}, /* Col 5: P1.13 */
    {.port = GPIO0, .pin = 3, .dt_flags = GPIO_ACTIVE_HIGH},  /* Col 6: P0.03 */
    {.port = GPIO1, .pin = 11, .dt_flags = GPIO_ACTIVE_HIGH}, /* Col 7: P1.11 */
};

const struct gpio_dt_spec rows[MATRIX_ROWS] = {
    {.port = GPIO0, .pin = 13, .dt_flags = GPIO_ACTIVE_HIGH}, /* Row 0: P0.13 */
    {.port = GPIO0, .pin = 24, .dt_flags = GPIO_ACTIVE_HIGH}, /* Row 1: P0.24 */
    {.port = GPIO0, .pin = 10, .dt_flags = GPIO_ACTIVE_HIGH}, /* Row 2: P0.10 */
    {.port = GPIO0, .pin = 4, .dt_flags = GPIO_ACTIVE_HIGH},  /* Row 3: P0.04 */
    {.port = GPIO0, .pin = 12, .dt_flags = GPIO_ACTIVE_HIGH}, /* Row 4: P0.12 */
    {.port = GPIO0, .pin = 7, .dt_flags = GPIO_ACTIVE_HIGH},  /* Row 5: P0.07 */
    {.port = GPIO1, .pin = 2, .dt_flags = GPIO_ACTIVE_HIGH},  /* Row 6: P1.02 */
    {.port = GPIO1, .pin = 4, .dt_flags = GPIO_ACTIVE_HIGH},  /* Row 7: P1.04 */
    {.port = GPIO1, .pin = 6, .dt_flags = GPIO_ACTIVE_HIGH},  /* Row 8: P1.06 */
};

const struct gpio_dt_spec encoders[ENCODER_PINS] = {
    {.port = GPIO0, .pin = 6, .dt_flags = GPIO_ACTIVE_LOW}, /* Row 0: P0.06 */
    {.port = GPIO0, .pin = 5, .dt_flags = GPIO_ACTIVE_LOW}, /* Row 1: P0.05 */
};
