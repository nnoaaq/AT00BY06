
#ifndef SENSORS_LED_H
#define SENSORS_LED_H

#include "onewire_library.h"
#include "pico/cyw43_arch.h"

#define DHT_PIN 15
#define DS18B20_PIN 22
#define LED_PIN 16

bool dht22_read(float *temperature, float *humidity);
bool wait_pin(uint pin, uint state);
bool DS18B20_read(OW *ow, float *temperature);

#endif