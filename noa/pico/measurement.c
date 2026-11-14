#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include <time.h>
#include "ntp.h"

#define KNOWN_RESISTANCE 220        // TUNNETUN "ETU"_VASTUKSEN OHMIT
#define CALIBRATED_TEMPERATURE 22   // TODETTU LÄMPÖTILA
#define CALIBRATED_RESISTANCE 331.5 // TODETTU VASTUS
#define MEASUREMENTS 10000          // MITTAUSTEN LUKUMÄÄRÄ

bool get_measured_temperature(char *buffer, size_t buffer_max_len)
{
    // *buffer = POINTTERI MIHIN JSON TALLENNETAAN
    const float conversionFactor = 3.3f / (1 << 12); // SYÖTTÖJÄNNITE / 4096
    float measuredVolts = 0.0f;
    // KERÄTÄÄN x-MÄÄRÄ MITTAUKSIA JA LASKETAAN KESKIARVO
    for (int i = 0; i < MEASUREMENTS; i++)
    {
        measuredVolts += adc_read() * conversionFactor;
    }
    // OTETAAN X-MÄÄRÄSTÄ MITTAUKSIA KESKIARVO
    float volt = measuredVolts / MEASUREMENTS;
    // MUUNNETAAN MITATTU JÄNNITE "SENSORIN" VASTUKSEKSI TUNNETUN (SARJAAN KYTKETTY) VASTUKSEN AVULLA
    float resistance = KNOWN_RESISTANCE * volt / (3.3 - volt);
    // MUUNNETAAN LASKETTU VASTUS LÄMPÖTILAKSI
    float temperature = CALIBRATED_TEMPERATURE + (resistance - CALIBRATED_RESISTANCE) / (0.00393 * CALIBRATED_RESISTANCE);

    // AJAN HAKEMINEN
    struct timespec ts;
    if (get_time(&ts) == 0)
    {
        snprintf(buffer, buffer_max_len,
                 "{\n"
                 "\"sensorId\":\"NoaPico_ID\","
                 "\"sensorName\":\"NoaPico\","
                 "\"temperature\":%.1f,\n"
                 "\"timeStamp\":%lld\n"
                 "}",
                 temperature, ts.tv_sec);

        return true;
    }
    else
    {
        printf("get_time palautti 0\n\n");
        return false;
    }
}