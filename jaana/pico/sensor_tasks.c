#include <stdio.h>
#include "pico/stdlib.h"
#include "sensor_task.h"
#include "sensors_led.h"
#include "measurements.h"
#include <math.h>

void run_sensor_measurement_cycle(OW *ow)
{
    float temperature = 0;
    float temperature2 = 0;
    float humidity = 0;

    float humidity_result = 0;
    float temperature_result = 0;

    uint reading = 0;
    uint attemp = 0;
    // static muuttuja tallennetaanpysyvään muistiin ja säilyy koko ohjelman ajan
    static float last_temperature = -999.0f;
    static float last_humidity = -999.0f;

    float final_temp_dht = 0;
    float final_hum = 0;

    const int MAX_ATTEMPS = 30;
    const int MAX_ROUNDS = 15;
    // Kerätään onnistuneita mittauksia (enintään 15 kpl tai 30 yritystä)
    while (reading < MAX_ROUNDS && attemp < MAX_ATTEMPS)
    {
        attemp++;
        // Luodaan DHT22
        if (dht22_read(&temperature, &humidity))
        {
            humidity_result += humidity;
            temperature_result += temperature;
            reading++;
            sleep_ms(3000);
        }
        else
        {
            printf("DHT22 reading epäonnistui\n");
            sleep_ms(2000);
        }
    }
    // Luodaan DS18B20
    if (DS18B20_read(ow, &temperature2))
        {
            printf("Lampotila ds18b20: %.1f C\n", temperature2);
        }
        else
        {
            printf("DS18B20 reading epäonnistui\n");
        }

    // Jos saatiin joitain mittauksia, lasketaan keskiarvot ja tallennetaan
    if (reading > 0)
    {
        final_temp_dht = temperature_result / reading;
        final_hum = humidity_result / reading;

        // Tallennetaan puskuriin, jos muutos on toleranssin ulkopuolella
        float differ_hum = last_humidity - final_hum;
        float differ_temp = last_temperature - final_temp_dht;

        if (fabsf(differ_temp) > 0.05f || fabsf(differ_hum) > 0.2f)
        {
            printf("Lampotila dht: %.1f C\n", final_temp_dht);
            printf("Kosteus: %.1f %%\n", final_hum);
            last_temperature = final_temp_dht;
            last_humidity = final_hum;
            save_measurement(final_temp_dht, final_hum, temperature2);
        printf("Measurement saved: %d/%d\n\n", measurement_count, MAX_MEASUREMENTS);
        printf("-------------------\n");
        }
    }
}