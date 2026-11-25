#include "keyboard.h"

#if defined(CONFIG_SOC_NRF52833_QIAA)
#define ENCODER_0 P1(5)
#else
#define ENCODER_0 P1(13)
#endif

const uint16_t pin_cols[MATRIX_COLS] = {
    P1(2),
    P1(0),
    P0(30),
    P0(6),
    P0(4),
    P0(12),
    P0(5),
    P0(8),
    P0(13),
    P0(24),
    P0(9),
    P0(10),
    P1(10),
    P0(3),
    P0(28),
};

const uint16_t pin_rows[MATRIX_ROWS] = {
    P0(29),
    P0(31),
    P0(22),
    P0(26),
    P0(7),
    P1(4),
};

const uint16_t pin_encoders[ENCODERS * ENCODER_PINS] = {
    ENCODER_0,
    P0(2),
};

const uint16_t pin_power_ext = P1(9);
