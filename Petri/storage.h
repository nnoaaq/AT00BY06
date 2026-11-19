#ifndef STORAGE_H
#define STORAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "measurement.h"

// Mittaushistorian pysyvä tallennus flash-muistiin.
//
// Historia on RAM-kopio (enintään MAX_MEASUREMENTS riviä), joka kirjoitetaan
// flashiin aina kokonaisena yhtenä sektorina. Flash-kirjoitus kuluttaa
// sektoria (tyypillisesti noin 100 000 tyhjennyskertaa), joten tallennusväliä
// ei kannata pitää 1 minuutissa pitkiä aikoja.

// Lataa historian flashista RAMiin ja tulostaa sen sarjaporttiin.
// Jos tallennus puuttuu tai on vioittunut, historia jää tyhjäksi.
void storage_load(void);

// Tyhjentää flash-sektorin. Kutsutaan ennen storage_load():ia.
bool storage_erase(void);

// Lisää mittauksen historian loppuun (täyttyessä vanhin poistuu) ja
// kirjoittaa historian flashiin. Palauttaa true vain, jos kirjoitus ja
// tarkistus onnistuivat. Epäonnistuessa historia pysyy ennallaan.
bool storage_append(const Measurement *measurement);

uint32_t storage_count(void);

// Palauttaa mittauksen indeksistä 0 (vanhin) .. count-1 (uusin).
const Measurement *storage_get(uint32_t index);

// Seuraavan minuuttilaskurin arvo: viimeisimmän tallennetun jälkeinen
// tai 1, jos historia on tyhjä.
uint32_t storage_next_minute(void);

// Tulostaa koko historian sarjaporttiin.
void storage_print_history(void);

#endif
