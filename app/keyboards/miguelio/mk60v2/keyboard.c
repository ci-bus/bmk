#include "keyboard.h"

#if defined(CONFIG_SOC_NRF52833_QIAA)
#define COL4_PIN P1(5)
#else
#define COL4_PIN P1(13)
#endif

const uint16_t pin_cols[MATRIX_COLS] = {
    P0(28),
    P0(15),
    P1(0),
    P0(10),
    COL4_PIN,
    P0(3),
    P0(2),
    P0(29),
};

const uint16_t pin_rows[MATRIX_ROWS] = {
    P0(13),
    P0(24),
    P0(9),
    P0(8),
    P0(12),
    P0(7),
    P1(2),
    P1(4),
    P0(17),
};

const uint16_t pin_encoders[ENCODERS * ENCODER_PINS] = {
    P0(6),
    P0(5),
};

const uint16_t pin_power_ext = P1(9);