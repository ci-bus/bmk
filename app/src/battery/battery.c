#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>

#include "battery.h"

static const struct adc_dt_spec adc_chan0 = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);

int32_t read_battery_voltage(void) {
    int err;
    uint16_t buf;
    struct adc_sequence sequence = {
        .buffer = &buf,
        .buffer_size = sizeof(buf),
    };

    adc_sequence_init_dt(&adc_chan0, &sequence);
    err = adc_read(adc_chan0.dev, &sequence);

    if (err < 0) {
        return err;
    }

    int32_t val_mv = buf;
    // Convertimos a mV usando la escala de Zephyr
    adc_raw_to_millivolts_dt(&adc_chan0, &val_mv);
    
    // Como usamos VDDHDIV5, el valor real es 5 veces lo que lee el ADC
    return val_mv * 5;
}

uint8_t calculate_battery_percentage(int32_t voltage_mv) {
    if (voltage_mv >= BAT_MAX_VOLT) {
        return 100;
    }
    if (voltage_mv <= BAT_MIN_VOLT) {
        return 0;
    }

    // Interpolación lineal: (V - Vmin) * 100 / (Vmax - Vmin)
    return (uint8_t)(((voltage_mv - BAT_MIN_VOLT) * 100) / (BAT_MAX_VOLT - BAT_MIN_VOLT));
}

uint8_t get_battery() {
    int32_t voltaje = read_battery_voltage();
    return calculate_battery_percentage(voltaje);
}