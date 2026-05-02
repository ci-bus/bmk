#ifndef BMK_BETTERY
#define BMK_BETTERY

#include <zephyr/types.h>

#ifndef BATTERY
#define BATTERY true
#endif
#ifndef BAT_UPDATE
#define BAT_UPDATE 4
#endif

int32_t read_battery_voltage(void);

uint8_t calculate_battery_percentage(int32_t voltage_mv);

uint8_t get_battery();

#endif