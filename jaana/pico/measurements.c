/* Tallennetaan RAMiin mittaukset ennen lähettämistä */
#include "measurements.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include "ntp.h"

measurement_t measurements[MAX_MEASUREMENTS];

int measurement_count = 0;


void save_measurement(float temperature, float humidity, float temperature2)
{
    if (measurement_count < MAX_MEASUREMENTS)   
    {
        measurements[measurement_count].dht_temperature = temperature;
        measurements[measurement_count].ds18b20_temperature = temperature2;
        measurements[measurement_count].humidity = humidity;
        measurements[measurement_count].timestamp = (long)current_epoch_time;
        printf("Aikaleima: %lld ms\n", (long long)measurements[measurement_count].timestamp);
        measurement_count++;
    }
}

