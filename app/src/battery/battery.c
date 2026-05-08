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
    uint8_t percent = 1;

    if (voltage < 3300)
    {
        return percent;
    }
    else if (voltage < 3500) // 10% 1/2 3300 - 3500
    {
        percent = (voltage - 3300) * 10 / 200;
    }
    else if (voltage < 3600) // 10% 1/1 3500 - 3600
    {
        percent = ((voltage - 3500) / 10) + 10;
    }
    else if (voltage < 3800) // 60% 6/2 3600 - 3800
    {
        percent = ((voltage - 3600) * 60 / 200) + 20;
    }
    else if (voltage < 3900) // 10% 1/1 3800 - 3900
    {
        percent = ((voltage - 3800) / 10) + 80;
    }
    else // 10% 1/2 3900 - 4100
    {
        percent = ((voltage - 3900) * 10 / 200) + 90;
    }
    return percent > 100 ? 100 : percent;
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
            smoothed_percent += BAT_SMOTHING * 2;
        }
        else if (percent < smoothed_percent)
        {
            smoothed_percent -= BAT_SMOTHING;
        }
        return (uint8_t)smoothed_percent;
    }
}
