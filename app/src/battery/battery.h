#ifndef BMK_BETTERY
#define BMK_BETTERY

#include <zephyr/types.h>

#ifndef BAT_MAX_VOLT
#define BAT_MAX_VOLT 4200
#endif
#ifndef BAT_MIN_VOLT
#define BAT_MIN_VOLT 3000
#endif
#ifndef BAT_UPDATE
#define BAT_UPDATE 300
#endif

int32_t read_battery_voltage(void);

uint8_t calculate_battery_percentage(int32_t voltage_mv);

uint8_t get_battery();

#endif