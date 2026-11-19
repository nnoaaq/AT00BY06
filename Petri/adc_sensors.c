#include "adc_sensors.h"

#include <math.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"

#include "config.h"

void adc_sensors_init(void)
{
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_gpio_init(BATTERY_ADC_PIN);
}

// Lukee ADC-tulon 16 kertaa ja palauttaa keskiarvon (12-bittinen arvo).
static uint16_t read_adc_average(unsigned input)
{
    adc_select_input(input);

    // Hylätään ensimmäinen näyte kanavan vaihtamisen jälkeen.
    (void)adc_read();
    sleep_us(10);

    uint32_t sum = 0;

    for (unsigned i = 0; i < 16; ++i)
    {
        sum += adc_read();
    }

    return (uint16_t)(sum / 16);
}

// Lämpötila lasketaan RP2040/RP2350:n tietolehden kaavalla:
// T = 27 - (U - 0,706 V) / 0,001721 V/°C.
// Tulos olettaa referenssijännitteeksi ADC_REFERENCE_V:n.
float adc_read_internal_temperature(void)
{
    uint16_t raw = read_adc_average(ADC_TEMP_SENSOR_INPUT);
    float volts = raw * ADC_REFERENCE_V / 4096.0f;

    return 27.0f - (volts - 0.706f) / 0.001721f;
}

// Akkujännite mitataan jännitteenjakajan kautta.
// BATTERY_DIVIDER kertoo, montako kertaa todellinen jännite on ADC-arvoa suurempi.
uint8_t adc_read_battery_percent(void)
{
    uint16_t raw = read_adc_average(BATTERY_ADC_INPUT);

    float voltage =
        raw * ADC_REFERENCE_V / 4096.0f * BATTERY_DIVIDER;

    float percent =
        (voltage - BATTERY_EMPTY_V) /
        (BATTERY_FULL_V - BATTERY_EMPTY_V) *
        100.0f;

    if (percent <= 0.0f)
    {
        return 0;
    }

    if (percent >= 100.0f)
    {
        return 100;
    }

    return (uint8_t)lroundf(percent);
}
