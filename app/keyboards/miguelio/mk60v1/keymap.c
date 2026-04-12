#include "keyboard.h"

/* Layers keycodes */
const uint16_t layers[LAYERS][MATRIX_COLS * MATRIX_ROWS + ENCODER_PINS] = {
    {
        // Col 0     Col 1         Col 2      Col 3      Col 4      Col 5      Col 6      Col 7
        HID_KEY_ESC, HID_KEY_NONE, HID_KEY_2, HID_KEY_4, HID_KEY_6, HID_KEY_8, HID_KEY_0, HID_KEY_EQUAL,                                                                                      // Row 0
        HID_KEY_1, HID_KEY_NONE, HID_KEY_3, HID_KEY_5, HID_KEY_7, HID_KEY_9, HID_KEY_MINUS, HID_KEY_BACKSPACE,                                                                                // Row 1
        HID_KEY_TAB, HID_KEY_Q, HID_KEY_E, HID_KEY_T, HID_KEY_U, HID_KEY_O, HID_KEY_LEFTBRACE, HID_KEY_BACKSLASH,                                                                             // Row 2
        HID_KEY_CAPSLOCK, HID_KEY_NONE, HID_KEY_W, HID_KEY_R, HID_KEY_Y, HID_KEY_I, HID_KEY_P, HID_KEY_RIGHTBRACE,                                                                            // Row 3
        HID_KEY_NONE, HID_KEY_A, HID_KEY_D, HID_KEY_G, HID_KEY_J, HID_KEY_L, HID_KEY_APOSTROPHE, HID_KEY_ENTER,                                                                               // Row 4
        HID_KEY_NONE, HID_KEY_NONE, HID_KEY_S, HID_KEY_F, HID_KEY_H, HID_KEY_K, HID_KEY_SEMICOLON, HID_KEY_BACKSLASH,                                                                         // Row 5
        HID_KEY_NONE, HID_KEY_Z, HID_KEY_C, HID_KEY_B, HID_KEY_M, HID_KEY_DOT, HID_KBD_MODIFIER_RIGHT_SHIFT, HID_KEY_SLASH,                                                                   // Row 6
        HID_KBD_MODIFIER_LEFT_SHIFT, HID_KEY_NUBS, HID_KEY_X, HID_KEY_V, HID_KEY_N, HID_KEY_COMMA, HID_KEY_LEFT, HID_KEY_UP,                                                                  // Row 7
        HID_KBD_MODIFIER_LEFT_CTRL, HID_KBD_MODIFIER_LEFT_UI, HID_KBD_MODIFIER_LEFT_ALT, HID_KEY_SPACE, HID_KBD_MODIFIER_RIGHT_ALT, HID_KBD_MODIFIER_RIGHT_CTRL, HID_KEY_DOWN, HID_KEY_RIGHT, // Row 8
        HID_KEY_VOLUME_DOWN, HID_KEY_VOLUME_UP                                                                                                                                                // Encoders
    },
    {
        // Col 0      Col 1         Col 2         Col 3         Col 4         Col 5         Col 6         Col 7
        HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, // Row 0
        HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, // Row 1
        HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, // Row 2
        HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, // Row 3
        HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, // Row 4
        HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, // Row 5
        HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, // Row 6
        HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, // Row 7
        HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, // Row 8
        HID_KEY_NONE, HID_KEY_NONE                                                                                      // Encoders
    }};