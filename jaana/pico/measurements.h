#ifndef MEASUREMENTS_H
#define MEASUREMENTS_H

#include "pico/stdlib.h"

#define MAX_MEASUREMENTS 5

typedef struct {
    float dht_temperature;
    float ds18b20_temperature;
    float humidity;
    uint32_t timestamp;
} measurement_t;

extern measurement_t measurements[MAX_MEASUREMENTS];

extern int measurement_count;

void save_measurement(float temperature, float humidity, float temperature2);

#endif