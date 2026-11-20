#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "measurements.h"
#include "sensors_led.h"

bool wait_pin(uint pin, uint state)
{
    absolute_time_t timeout = make_timeout_time_us(1000);
    // tarkasteellaan pinnin tilaa kunnes se vastaa haluttua tilaa
    while (gpio_get(pin) != state)
    {
        // Check if the specified timestamp has been reached on the default timer instance.
        if (time_reached(timeout)) return false;
    }
    return true;
}
bool dht22_read(float *temperature, float *humidity)
{
    uint8_t data[5] = {0};

    // Käynnistetään mittaus
    gpio_set_dir(DHT_PIN, GPIO_OUT);
    gpio_put(DHT_PIN, 0);
    busy_wait_ms(2);

    gpio_put(DHT_PIN, 1);
    busy_wait_us(30);

    gpio_set_dir(DHT_PIN, GPIO_IN);
    gpio_pull_up(DHT_PIN);

    // Odotetaan anturin vastaussignaalia, jonka jälkeen voidaan siirtyä datan lukemiseen.

    if (!wait_pin(DHT_PIN, 0)) return false;
    if (!wait_pin(DHT_PIN, 1)) return false;
    if (!wait_pin(DHT_PIN, 0)) return false;

    // Luetaan dht:n lähettämät 40 databittiä
    for (int i = 0; i < 40; i++)
    {
        // Odotetaan, että anturin datalinja siirtyy HIGH-tilaan.
        if (!wait_pin(DHT_PIN, 1)) return false;
        busy_wait_us(40);
        // siirretään datan bittejä vasemmalle, että saadaan tilaa seuraaville
        data[i / 8] <<= 1;
        // jos pinni on high, niin lisätään tavun viimeiseksi bitiksi 1
        if (gpio_get(DHT_PIN))
        {
            data[i / 8] |= 1;
        }
        // odotetaan että pinni on 0
        if (!wait_pin(DHT_PIN, 0)) return false;
    }

    // Tarkistussumma
    if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4])
    {
        return false;
    }

    // Kosteus on 2 ekaa dataa
    *humidity = ((data[0] << 8) | data[1]) / 10.0f;

    // Lämpötila on 2 viimeistä dataa
    int16_t temp = ((data[2] & 0x7F) << 8) | data[3];

    *temperature = temp / 10.0f;
    // jos korkein bitti on 1, niin lämpötila on negatiivinen
    if (data[2] & 0x80)
    {
        *temperature = -*temperature;
    }

    return true;
}
