#include "dht22.h"

#include <stdint.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"

#include "config.h"

void dht22_init(void)
{
    gpio_init(DHT_PIN);
    gpio_set_dir(DHT_PIN, GPIO_IN);
    gpio_pull_up(DHT_PIN);
}

// Odottaa, että nasta saavuttaa halutun tason. Palauttaa false, jos
// tasoa ei saavuteta aikarajan sisällä.
static bool wait_pin_level(bool level, uint32_t timeout_us)
{
    uint64_t started = time_us_64();

    while ((bool)gpio_get(DHT_PIN) != level)
    {
        if (time_us_64() - started >= timeout_us)
        {
            return false;
        }
    }

    return true;
}

// DHT22 palauttaa lämpötilan ja kosteuden kymmenkertaisena kokonaislukuna.
// Siksi arvot jaetaan 10:llä ennen käyttöä.
//
// Sanoma on 5 tavua: kosteus (2), lämpötila (2) ja tarkistussumma (1).
// Jokainen bitti alkaa noin 50 µs matalalla tasolla, ja sen jälkeen
// korkean tason pituus kertoo arvon: lyhyt (~27 µs) = 0, pitkä (~70 µs) = 1.
bool dht22_read(float *temperature, float *humidity)
{
    uint8_t data[5] = {0};

    // Ajetaan vain nolla aktiivisesti.
    // Ykkönen syntyy vapauttamalla linja tuloksi.
    // Matala pulssi (väh. 1 ms) käynnistää anturin.
    gpio_put(DHT_PIN, 0);
    gpio_set_dir(DHT_PIN, GPIO_OUT);
    sleep_ms(2);

    // Bittien ajoitus on mikrosekuntitasolla, joten keskeytykset
    // pidetään pois lukemisen ajan.
    uint32_t irq_state = save_and_disable_interrupts();

    gpio_set_dir(DHT_PIN, GPIO_IN);

    // Anturin vastaus:
    // noin 80 us LOW, noin 80 us HIGH, sitten databittien LOW.
    if (!wait_pin_level(false, 150) ||
        !wait_pin_level(true, 150) ||
        !wait_pin_level(false, 150))
    {
        restore_interrupts(irq_state);
        return false;
    }

    for (unsigned bit = 0; bit < 40; ++bit)
    {
        if (!wait_pin_level(true, 100))
        {
            restore_interrupts(irq_state);
            return false;
        }

        uint64_t high_started = time_us_64();

        if (!wait_pin_level(false, 120))
        {
            restore_interrupts(irq_state);
            return false;
        }

        uint64_t high_us = time_us_64() - high_started;

        data[bit / 8] <<= 1;

        // Raja 45 µs on lyhyen ja pitkän pulssin puolivälissä.
        if (high_us > 45)
        {
            data[bit / 8] |= 1;
        }
    }

    restore_interrupts(irq_state);

    uint8_t checksum =
        (uint8_t)(data[0] + data[1] + data[2] + data[3]);

    if (checksum != data[4])
    {
        return false;
    }

    uint16_t humidity_raw =
        ((uint16_t)data[0] << 8) | data[1];

    // Lämpötilan ylin bitti on etumerkki (sign-magnitude, ei kahden komplementti).
    uint16_t temperature_raw =
        ((uint16_t)(data[2] & 0x7F) << 8) | data[3];

    float t = temperature_raw / 10.0f;

    if (data[2] & 0x80)
    {
        t = -t;
    }

    *temperature = t;
    *humidity = humidity_raw / 10.0f;

    return true;
}
