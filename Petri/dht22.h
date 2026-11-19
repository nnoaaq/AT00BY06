#ifndef DHT22_H
#define DHT22_H

#include <stdbool.h>

// DHT22 / AM2302 -lämpötila- ja kosteusanturi (yhden johtimen protokolla).

// Alustaa datanastan tuloksi sisäisellä ylösvedolla.
void dht22_init(void);

// Lukee anturin. Palauttaa true ja täyttää arvot (°C ja %RH), jos luku ja
// tarkistussumma onnistuivat. Kutsu kestää noin 7 ms, ja siitä noin 5 ms
// keskeytykset ovat pois päältä (bittien ajoitus on mikrosekuntitasolla).
bool dht22_read(float *temperature, float *humidity);

#endif
