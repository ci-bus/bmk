#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>

#include "battery.h"

#include <zephyr/drivers/adc.h>

static const struct adc_dt_spec adc_chan0 = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);

// Variable para asegurar que solo configuramos el canal una vez
static bool adc_configured = false;

int32_t read_battery_voltage(void) {
    int rc;
    int32_t val;
    uint16_t buf;

    if (!device_is_ready(adc_chan0.dev)) {
        return -ENODEV;
    }

    if (!adc_configured) {
        rc = adc_channel_setup_dt(&adc_chan0);
        if (rc != 0) {
            return rc;
        }
        adc_configured = true;
    }

    struct adc_sequence sequence = {
        .buffer = &buf,
        .buffer_size = sizeof(buf),
        .oversampling = 4,
        .calibrate = false,
    };
    
    adc_sequence_init_dt(&adc_chan0, &sequence);

    rc = adc_read(adc_chan0.dev, &sequence);
    if (rc != 0) {
        return rc;
    }

    val = (int32_t)buf;

    rc = adc_raw_to_millivolts_dt(&adc_chan0, &val);
    if (rc != 0) {
        return rc;
    }

    return val * 5;
}

uint8_t calculate_battery_percentage(int32_t voltage)
{
    uint8_t result = 100;
    int8_t percent = (voltage * 2 / 15) - 450;
    if (percent > 100) {
        result = 100;
    } else if (percent < 0) {
        result = 0;
    } else {
        result = (uint8_t) percent;
    }

    return result;
}

uint8_t get_battery()
{
    int32_t voltaje = read_battery_voltage();
    return calculate_battery_percentage(voltaje);
}