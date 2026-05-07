#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>

#include "battery.h"

static float smoothed_percent = 0;
static const struct adc_dt_spec adc_chan0 = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);
static bool bat_first_read = true;

// Variable para asegurar que solo configuramos el canal una vez
static bool adc_configured = false;

int32_t read_battery_voltage(void)
{
    int rc;
    int32_t val;
    uint16_t buf;

    if (!device_is_ready(adc_chan0.dev))
    {
        return -ENODEV;
    }

    if (!adc_configured)
    {
        rc = adc_channel_setup_dt(&adc_chan0);
        if (rc != 0)
        {
            return rc;
        }
        adc_configured = true;
    }

    struct adc_sequence sequence = {
        .buffer = &buf,
        .buffer_size = sizeof(buf),
        .oversampling = 2,
        .calibrate = bat_first_read,
    };

    adc_sequence_init_dt(&adc_chan0, &sequence);

    adc_read(adc_chan0.dev, &sequence);

    val = (int32_t)buf;

    val = (val * 6000) >> 12;

    return val > 1 ? val : 1;
}

uint8_t calculate_battery_percentage(int32_t voltage)
{
    uint8_t percent;

    if (voltage > BAT_MAX)
    {
        percent = 100;
    }
    else if (voltage < BAT_MIN)
    {
        percent = 1;
    }
    else
    {
        percent = ((voltage - BAT_MIN) * 100) / (BAT_MAX - BAT_MIN);
    }

    return percent;
}

uint8_t get_battery()
{
    int32_t voltaje = read_battery_voltage();
    uint8_t percent = calculate_battery_percentage(voltaje);

    if (bat_first_read)
    {
        bat_first_read = false;
        smoothed_percent = percent;
        return percent;
    }
    else
    {
        if (percent > smoothed_percent)
        {
            smoothed_percent += (BAT_SMOTHING * 5);
        }
        else if (percent < smoothed_percent)
        {
            smoothed_percent -= BAT_SMOTHING;
        }
        return (uint8_t)smoothed_percent;
    }
}
