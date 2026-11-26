#include "measurements.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include "ntp.h"

measurement_t measurements[MAX_MEASUREMENTS];

int measurement_count = 0;

void save_measurement(float temperature, float humidity, float temperature2)
{
    if (MAX_MEASUREMENTS <= 0)
    {
        return;
    }

    int idx;
    if (measurement_count < MAX_MEASUREMENTS)
    {
        idx = measurement_count;
        measurement_count++;
    }
    else
    {
        // Pudotetaan vanhin mittaus ja siirretään loput vasemmalle
        // memmove(kohde, lähde, tavumäärä) lähde = tästä indeksistä eteenpäin
        memmove(&measurements[0], &measurements[1], sizeof(measurement_t) * (MAX_MEASUREMENTS - 1));
        idx = MAX_MEASUREMENTS - 1;
    }

    measurements[idx].dht_temperature = temperature;
    measurements[idx].ds18b20_temperature = temperature2;
    measurements[idx].humidity = humidity;
    // TEE: muuta että hakee tässä ajan
    measurements[idx].timestamp = (long)current_epoch_time;

    printf("Aikaleima: %lu\n", (unsigned long)measurements[idx].timestamp);
}