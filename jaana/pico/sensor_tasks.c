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
    float temperature2_result = 0;

    uint mittaus = 0;
    uint mittaus2 = 0;
    uint yritykset = 0;
    float last_temperature = -999;
    float last_humidity = -999;
    float differ_hum = -999;
    float differ_temp = -999;

    float final_temp_dht = 0;
    float final_hum = 0;
    float final_temp_ds = 0;

    bool dht_changed = false;
    bool ds_changed = false;

    const int MAX_YRITYKSET = 30; // Estetään ikuinen jumittuminen, jos anturit rikkoutuvat
    const int MITTAUKSET = 10;

    // Kerätään onnistuneita mittauksia
    while (mittaus < MITTAUKSET && yritykset < MAX_YRITYKSET)
    {
        yritykset++;

        // 1. Luodaan DS18B20
        if (DS18B20_read(ow, &temperature2))
        {
            temperature2_result += temperature2;
            mittaus2++;
        }
        else
        {
            printf("DS18B20 mittaus epäonnistui\n");
        }

        // 2. Luodaan DHT22
        if (dht22_read(&temperature, &humidity))
        {
            humidity_result += humidity;
            temperature_result += temperature;
            mittaus++;
            sleep_ms(3000);
        }
        else
        {
            printf("DHT22 mittaus epäonnistui\n");
            sleep_ms(2000);
        }
    }

    // Jos saatiin joitain mittauksia, lasketaan keskiarvot ja tallennetaan
    if (mittaus2 > 0)
    {
        ds_changed = true;
        final_temp_ds = temperature2_result / mittaus2;

        printf("Lampotila ds18b20: %.1f C\n", final_temp_ds);
    }
    if (mittaus > 0)
    {
        final_temp_dht = temperature_result / mittaus;
        final_hum = humidity_result / mittaus;

        // Tallennetaan puskuriin, jos muutos on toleranssin ulkopuolella
        differ_hum = last_humidity - final_hum;
        differ_temp = last_temperature - final_temp_dht;
        if (fabsf(differ_temp) > 0.1f || fabsf(differ_hum) > 0.5f)
        {
            dht_changed = true;
            printf("Lampotila dht: %.1f C\n", final_temp_dht);
            printf("Kosteus: %.1f %%\n", final_hum);
            last_temperature = final_temp_dht;
            last_humidity = final_hum;
        }
    }

    if (dht_changed || ds_changed)
    {
        save_measurement(final_temp_dht, final_hum, final_temp_ds);
        printf("Measurement saved: %d/%d\n\n", measurement_count, MAX_MEASUREMENTS);
        printf("-------------------\n");
    }
}