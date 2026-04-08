#ifndef BMK_H
#define BMK_H

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#define HID_KEY_NONE 0
#define HID_KEY_NUBS 0x64
#define HID_KEY_VOLUME_UP 0x00E9
#define HID_KEY_VOLUME_DOWN 0x00EA
#define HID_KEY_MUTE 0x00E2
#define HID_KEY_PLAY_PAUSE 0x00CD
#define HID_KEY_NEXT_TRACK 0x00B5
#define HID_KEY_PREV_TRACK 0x00B6
#define HID_KEY_STOP 0x00B7
#define HID_KEY_EJECT 0x00B8
#define HID_KEY_SLEEP 0x00E2

#define ENCODER_PINS 2

extern const struct gpio_dt_spec cols[];
extern const struct gpio_dt_spec rows[];
extern const struct gpio_dt_spec encoder[ENCODER_PINS];

#define GPIO0 DEVICE_DT_GET(DT_NODELABEL(gpio0))
#define GPIO1 DEVICE_DT_GET(DT_NODELABEL(gpio1))

#endif