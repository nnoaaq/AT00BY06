#include "storage.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/flash.h"
#include "hardware/flash.h"

#include "ntp_clock.h"

// Flash-tallennusalue alkaa 2 MiB:n kohdalta.
// Tämä on erillinen alue, jotta se ei sekoitu ohjelmakoodiin.
#define FLASH_TARGET_OFFSET (2u * 1024u * 1024u)

// Magic ja versio tunnistavat kelvollisen tallennuksen.
// Checksum varmistaa, ettei flash-data ole vioittunut.
#define STORAGE_MAGIC 0x53414132u
#define STORAGE_VERSION 2u

// Flash-sektorin sisältö: otsake ja heti sen perässä mittaukset peräkkäin.
// Sektorin loppu täytetään 0xFF-tavuilla (tyhjän flashin arvo).
typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t count;
    uint32_t checksum; // mittausdatan tarkistussumma
} StorageHeader;

_Static_assert(
    sizeof(StorageHeader) +
            MAX_MEASUREMENTS * sizeof(Measurement) <=
        FLASH_SECTOR_SIZE,
    "Mittaukset eivat mahdu flash-sektoriin");

_Static_assert(
    FLASH_TARGET_OFFSET % FLASH_SECTOR_SIZE == 0,
    "Flash-osoitteen pitaa olla sektorin rajalla");

_Static_assert(
    FLASH_TARGET_OFFSET + FLASH_SECTOR_SIZE <= PICO_FLASH_SIZE_BYTES,
    "Tallennusalue ei mahdu kortin flash-muistiin");

static Measurement measurements[MAX_MEASUREMENTS];
static uint32_t measurement_count = 0;

// Flash-kirjoitus kirjoittaa aina koko sektorin, joten uusi sisältö
// rakennetaan ensin tähän puskuriin.
static uint8_t flash_buffer[FLASH_SECTOR_SIZE];

// Flash-sektorin sisältö luetaan suoraan XIP-muistialueelta.
static const uint8_t *flash_contents(void)
{
    return (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET);
}

// Yksinkertainen tarkistussumma (FNV-1a), ei salaustarkoitukseen.
static uint32_t data_checksum(const void *data, size_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t hash = 2166136261u;

    for (size_t i = 0; i < length; ++i)
    {
        hash ^= bytes[i];
        hash *= 16777619u;
    }

    return hash;
}

// Flash-operaatiot ajetaan flash_safe_execute:lla. Callbackin pitää olla
// RAMissa (__not_in_flash_func), koska flash ei ole luettavissa
// tyhjennyksen ja ohjelmoinnin aikana.
static void __not_in_flash_func(flash_write_callback)(void *param)
{
    const uint8_t *buffer = (const uint8_t *)param;

    flash_range_erase(
        FLASH_TARGET_OFFSET,
        FLASH_SECTOR_SIZE);

    flash_range_program(
        FLASH_TARGET_OFFSET,
        buffer,
        FLASH_SECTOR_SIZE);
}

static void __not_in_flash_func(flash_erase_callback)(void *param)
{
    (void)param;

    flash_range_erase(
        FLASH_TARGET_OFFSET,
        FLASH_SECTOR_SIZE);
}

bool storage_erase(void)
{
    int result = flash_safe_execute(
        flash_erase_callback,
        NULL,
        2000);

    if (result != PICO_OK)
    {
        printf("Flash-tyhjennys epaonnistui: %d\n", result);
        return false;
    }

    return true;
}

// Kirjoittaa flash_buffer-puskurin flashiin ja varmistaa lukemalla takaisin.
static bool write_flash_buffer(void)
{
    int result = flash_safe_execute(
        flash_write_callback,
        flash_buffer,
        2000);

    if (result != PICO_OK)
    {
        printf("Flash-kirjoitus epaonnistui: %d\n", result);
        return false;
    }

    if (memcmp(flash_contents(), flash_buffer, sizeof(flash_buffer)) != 0)
    {
        printf("Flash-kirjoituksen tarkistus epaonnistui\n");
        return false;
    }

    return true;
}

bool storage_append(const Measurement *measurement)
{
    // Täyttyneestä historiasta pudotetaan vanhin pois.
    uint32_t skip = (measurement_count == MAX_MEASUREMENTS) ? 1 : 0;
    uint32_t keep = measurement_count - skip;
    uint32_t new_count = keep + 1;

    // Uusi sisältö rakennetaan flash_buffer-puskuriin koskematta RAM-historiaan.
    // Näin epäonnistunut kirjoitus ei vaadi mitään palautusta.
    uint8_t *data = flash_buffer + sizeof(StorageHeader);
    size_t data_length = new_count * sizeof(Measurement);

    memset(flash_buffer, 0xFF, sizeof(flash_buffer));
    memcpy(data, &measurements[skip], keep * sizeof(Measurement));
    memcpy(data + keep * sizeof(Measurement), measurement, sizeof(Measurement));

    StorageHeader header = {
        .magic = STORAGE_MAGIC,
        .version = STORAGE_VERSION,
        .count = new_count,
        .checksum = data_checksum(data, data_length)};

    memcpy(flash_buffer, &header, sizeof(header));

    if (!write_flash_buffer())
    {
        return false;
    }

    // Flash päivitetty: päivitetään RAM-kopio vastaamaan sitä.
    if (skip)
    {
        memmove(measurements, measurements + 1, keep * sizeof(Measurement));
    }

    measurements[keep] = *measurement;
    measurement_count = new_count;

    return true;
}

uint32_t storage_count(void)
{
    return measurement_count;
}

const Measurement *storage_get(uint32_t index)
{
    return index < measurement_count ? &measurements[index] : NULL;
}

uint32_t storage_next_minute(void)
{
    if (measurement_count == 0)
    {
        return 1;
    }

    return measurements[measurement_count - 1].minute + 1;
}

void storage_print_history(void)
{
    printf(
        "\n--- Tallennetut mittaukset: %" PRIu32 " ---\n",
        measurement_count);

    for (uint32_t i = 0; i < measurement_count; ++i)
    {
        const Measurement *m = &measurements[i];

        char timestamp[32];

        ntp_clock_format_saved(
            m->unix_seconds,
            timestamp,
            sizeof(timestamp));

        printf(
            "Minuutti %" PRIu32
            " | Aika %s"
            " | Ulko %.1f C"
            " | Kosteus %.1f %%"
            " | Pico %.1f C"
            " | Akku %u %% | Jakso %u min\n",

            m->minute,
            timestamp,

            m->temperature_x10 / 10.0,
            m->humidity_x10 / 10.0,
            m->internal_temperature_x10 / 10.0,

            (unsigned)m->battery_percent,
            m->interval_minutes ? (unsigned)m->interval_minutes : 1u);
    }
    fflush(stdout);
}

void storage_load(void)
{
    const uint8_t *stored = flash_contents();

    StorageHeader header;
    memcpy(&header, stored, sizeof(header));

    measurement_count = 0;

    if (header.magic != STORAGE_MAGIC ||
        header.version != STORAGE_VERSION ||
        header.count > MAX_MEASUREMENTS)
    {
        printf("Ei taman version kelvollisia tallennuksia\n");
        return;
    }

    size_t data_length = header.count * sizeof(Measurement);
    const uint8_t *data = stored + sizeof(header);

    if (data_checksum(data, data_length) != header.checksum)
    {
        printf("Flash-datan tarkistussumma ei tasmaa\n");
        return;
    }

    memcpy(measurements, data, data_length);
    measurement_count = header.count;

    storage_print_history();
}
