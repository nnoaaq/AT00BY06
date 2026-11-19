#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include <stdint.h>

// Flash-muistiin tallennetaan enintään 120 viimeisintä mittausta.
// Kun historia täyttyy, vanhin poistuu ja uusin lisätään loppuun.
#define MAX_MEASUREMENTS 120

// Yksi tallennettu mittaus (tallennusjakson keskiarvo).
// Rakenne tallennetaan sellaisenaan flashiin, joten sen koko ja kenttien
// järjestys on lukittu. Muutos vaatii STORAGE_VERSIONin kasvattamisen.
typedef struct
{
    uint32_t minute;       // minuuttilaskurin arvo jakson päättyessä
    uint32_t unix_seconds; // UTC-aika; 0 = SNTP-aikaa ei ollut saatavilla

    int16_t temperature_x10;          // ulkolämpötila, 0,1 °C:n tarkkuudella
    uint16_t humidity_x10;            // ilmankosteus, 0,1 %:n tarkkuudella
    int16_t internal_temperature_x10; // Picon sisäinen lämpötila

    uint8_t battery_percent;
    uint8_t interval_minutes; // tallennusjakson pituus (1, 15 tai 60 min)
} Measurement;

_Static_assert(sizeof(Measurement) == 16,
               "Measurement-rakenteen koko on odottamaton");

#endif
