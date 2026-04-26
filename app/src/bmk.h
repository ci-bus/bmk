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
#include <hal/nrf_power.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/led_strip.h>

#include "hid.h"

#define ENCODER_PINS 2

extern const struct gpio_dt_spec cols[];
extern const struct gpio_dt_spec rows[];
extern const struct gpio_dt_spec encoders[ENCODER_PINS];
extern const struct gpio_dt_spec power_ext;

#define GPIO0 DEVICE_DT_GET(DT_NODELABEL(gpio0))
#define GPIO1 DEVICE_DT_GET(DT_NODELABEL(gpio1))

#endif