#include <stdint.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "measurements.h"
#include "onewire_library.h"
#include "ow_rom.h"
#include "ds18b20.h"
#include "sensors_led.h"

bool setup_onewire(OW *ow, PIO pio)
{
    if (!pio_can_add_program(pio, &onewire_program))
    {
        printf("Virhe: PIO-ohjelmaa ei voi lisätä!\n");
        return false;
    }

    uint offset = pio_add_program(pio, &onewire_program);
    if (!ow_init(ow, pio, offset, DS18B20_PIN))
    {
        printf("Virhe: 1-Wire-alustus epäonnistui!\n");
        return false;
    }

    printf("1-Wire alustettu pinniin GPIO %d\n", DS18B20_PIN);
    return true;
}

// Tämä funktio laskee ns. CRC8-tarkistussumman (Cyclic Redundancy Check) saaduille datatavuille.
// Se varmistaa, että anturilta luettu data ei ole vioittunut matkalla (esim. sähköhäiriön takia). Algoritmi laskee datasta tarkistustavun, jota verrataan anturin lähettämään tarkistustavuun.
static uint8_t ds18b20_crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0;

    for (int i = 0; i < len; i++)
    {
        uint8_t inbyte = data[i];

        for (int j = 0; j < 8; j++)
        {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;

            if (mix)
            {
                crc ^= 0x8C;
            }

            inbyte >>= 1;
        }
    }

    return crc;
}

bool DS18B20_read(OW *ow, float *temperature)
{
    uint8_t data[9] = {0};

    // Käynnistetään mittaus
    // ow_reset(ow): Lähettää väylälle nollaussignaalin tarkistaakseen, 
    // että anturi on paikalla ja vastaa. Jos anturia ei löydy, tulostetaan virhe ja palautetaan false.
    if (!ow_reset(ow))
    {
        printf("DS18B20 not detected!\n");
        return false;
    }
    // ow_send(ow, OW_SKIP_ROM): Ohitetaan yksittäisen anturin osoitteen haku (käytetään silloin, kun väylässä on vain yksi anturi).
    ow_send(ow, OW_SKIP_ROM);
    //  ow_send(ow, DS18B20_CONVERT_T): Käsketään anturia aloittamaan lämpötilan mittaus ja muunnos.
    ow_send(ow, DS18B20_CONVERT_T);

    // 12-bit resolution: maximum conversion time 750 ms
    sleep_ms(750);

    // Lue data
    if (!ow_reset(ow))
    {
        printf("DS18B20 disconnected!\n");
        return false;
    }


// Lähetetään komennot OW_SKIP_ROM ja DS18B20_READ_SCRATCHPAD 
// (käsky pyytää anturia lähettämään sen sisäisen muistin eli scratchpadin sisällön).
    ow_send(ow, OW_SKIP_ROM);
    ow_send(ow, DS18B20_READ_SCRATCHPAD);

    for (int i = 0; i < 9; i++)
    {
        data[i] = ow_read(ow);
    }
    // Tarkista CRC
    if (ds18b20_crc8(data, 8) != data[8])
    {
        printf("DS18B20 CRC error!\n");
        return false;
    }
    // Muodosta 16-bit signed lämpötila-arvo
    // Lämpötila on jaettu kahteen tavuun (data[0] ja data[1]).
    int16_t raw_temperature = (int16_t)((data[1] << 8) | data[0]);

    *temperature = raw_temperature / 16.0f;

    return true;
}
