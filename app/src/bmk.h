#ifndef BMK_H
#define BMK_H

#include <zephyr/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/pm/device.h>
#include <hal/nrf_power.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/reboot.h>
#include <hal/nrf_gpio.h>

#include "hid.h"

#define ENCODER_PINS 2

#define P0(pin) (0x0000 | (pin & 0xFF))
#define P1(pin) (0x0100 | (pin & 0xFF))

extern const uint16_t pin_cols[];
extern const uint16_t pin_rows[];
extern const uint16_t pin_encoders[ENCODER_PINS];
extern const uint16_t pin_power_ext;

#define GPIO0 DEVICE_DT_GET(DT_NODELABEL(gpio0))
#define GPIO1 DEVICE_DT_GET(DT_NODELABEL(gpio1))

#endif