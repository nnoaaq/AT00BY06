#ifndef ADC_SENSORS_H
#define ADC_SENSORS_H

#include <stdint.h>

// Picon ADC:llä mitattavat arvot: akkujännite ja sisäinen lämpötila.

// Alustaa ADC:n, sisäisen lämpötila-anturin ja akkunastan.
void adc_sensors_init(void);

// Picon sisäinen lämpötila (°C).
float adc_read_internal_temperature(void);

// Akun varaustaso 0–100 % (lineaarinen arvio jännitteestä).
uint8_t adc_read_battery_percent(void);

#endif
