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

    // PASO 1: Configuración inicial (Equivalente al vddh_init de ZMK)
    if (!adc_configured) {
        rc = adc_channel_setup_dt(&adc_chan0);
        if (rc != 0) {
            return rc; // Si devuelve 22 aquí, el overlay tiene un valor no soportado
        }
        adc_configured = true;
    }

    // PASO 2: Preparar la secuencia (Equivalente a vddh_sample_fetch)
    struct adc_sequence sequence = {
        .buffer = &buf,
        .buffer_size = sizeof(buf),
        // ZMK usa oversampling para estabilizar la lectura
        .oversampling = 4,
        .calibrate = false,
    };
    
    // Inicializamos con los valores del DeviceTree (resolución 12, etc.)
    adc_sequence_init_dt(&adc_chan0, &sequence);

    // PASO 3: Lectura física
    rc = adc_read(adc_chan0.dev, &sequence);
    if (rc != 0) {
        return rc;
    }

    // PASO 4: Conversión a milivoltios
    val = (int32_t)buf;
    
    // Usamos la fórmula de conversión de Zephyr
    // Voltaje de entrada = $val \cdot \frac{Ref}{Resolución \cdot Ganancia}$
    rc = adc_raw_to_millivolts_dt(&adc_chan0, &val);
    if (rc != 0) {
        return rc;
    }

    // PASO 5: Multiplicar por el divisor (VDDHDIV5)
    return val * 5;
}

uint8_t calculate_battery_percentage(int32_t voltage_mv)
{
    // 4200mV es el estándar para Li-Po cargada al 100%
    if (voltage_mv >= 4200)
    {
        return 100;
    }

    // ZMK usa 3450mV como el límite inferior (0%) para evitar descargas profundas
    if (voltage_mv <= 3450)
    {
        return 0;
    }

    /*
     * Fórmula de ZMK: (bat_mv * 2 / 15) - 459
     * Esta fórmula es una aproximación eficiente que no usa números de punto flotante.
     * Ejemplo para 3.7V (3700mV):
     * (3700 * 2 / 15) - 459
     * (7400 / 15) - 459 = 493 - 459 = 34%
     */
    int32_t pct = (voltage_mv * 2 / 15) - 459;

    // Aseguramos que el valor esté en el rango 0-100 por seguridad
    if (pct < 0)
        return 0;
    if (pct > 100)
        return 100;

    return (uint8_t)pct;
}

uint8_t get_battery()
{
    int32_t voltaje = read_battery_voltage();
    return calculate_battery_percentage(voltaje);
}