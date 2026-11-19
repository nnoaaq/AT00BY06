#include "ntp_clock.h"

#include <inttypes.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/sync.h"

#include "lwipopts.h" // saaasema_set_ntp_time()-esittely

static bool ntp_valid = false;
static uint32_t ntp_base_seconds = 0; // NTP:ltä saatu Unix-aika
static uint64_t ntp_base_us = 0;      // time_us_64() sillä hetkellä

// lwIP:n SNTP kutsuu tätä, kun NTP-vastaus on saapunut.
// Tallennetaan saatu Unix-aika ja hetki, jolloin se saatiin.
void saaasema_set_ntp_time(uint32_t unix_seconds)
{
    uint32_t irq_state = save_and_disable_interrupts();

    ntp_base_seconds = unix_seconds;
    ntp_base_us = time_us_64();
    ntp_valid = true;

    restore_interrupts(irq_state);

    printf(
        "SNTP: aika vastaanotettu, Unix-aika %" PRIu32 "\n",
        unix_seconds);
}

// Palauttaa nykyisen UTC-ajan, jos NTP on synkronoitu.
// Aika lasketaan: NTP-perusaika + kulunut aika.
bool ntp_clock_read_utc(time_t *result)
{
    uint32_t irq_state = save_and_disable_interrupts();

    bool valid = ntp_valid;
    uint32_t base_seconds = ntp_base_seconds;
    uint64_t elapsed_us = time_us_64() - ntp_base_us;

    restore_interrupts(irq_state);

    if (!valid)
    {
        return false;
    }

    *result = (time_t)((uint64_t)base_seconds + elapsed_us / 1000000ULL);

    return true;
}

// Palauttaa true, jos Suomessa on kesäaika annetulla UTC-hetkellä.
//
// EU:n sääntö: kesäaika alkaa maaliskuun viimeisenä sunnuntaina klo 01:00 UTC
// ja päättyy lokakuun viimeisenä sunnuntaina klo 01:00 UTC. Siirtymä on
// kaikissa EU-maissa sama UTC-hetki, joten päättely tehdään UTC-ajasta.
//
// Parametri on UTC-aika struct tm -muodossa (tm_mon 0–11, tm_wday 0 = sunnuntai).
static bool finland_dst(const struct tm *utc)
{
    int month = utc->tm_mon;

    // Marraskuu–helmikuu: aina normaaliaika.
    if (month < 2 || month > 9)
    {
        return false;
    }

    // Huhtikuu–syyskuu: aina kesäaika.
    if (month > 2 && month < 9)
    {
        return true;
    }

    // Maaliskuu (2) ja lokakuu (9) ratkaistaan päivän ja kellonajan mukaan.
    //
    // tm_mday - tm_wday on viimeisimmän sunnuntain päivämäärä (kuun alussa
    // se voi olla nolla tai negatiivinen). Kuun viimeinen sunnuntai osuu
    // aina päiville 25–31, joten jos viimeisin sunnuntai on päivä 25 tai
    // myöhemmin, se on kuun viimeinen sunnuntai ja ollaan sen päivänä tai
    // sen jälkeen.
    //
    // Sunnuntaina itsessään siirtymä tapahtuu vasta klo 01:00 UTC, joten
    // sitä aiemmin sunnuntai kuuluu vielä edelliseen aikaan.
    bool after_last_sunday_transition =
        (utc->tm_mday - utc->tm_wday >= 25) &&
        (utc->tm_wday != 0 || utc->tm_hour >= 1);

    // Maaliskuussa kesäaika alkaa siirtymästä, lokakuussa se päättyy siihen.
    if (month == 2)
    {
        return after_last_sunday_transition;
    }

    return !after_last_sunday_transition;
}

// Muuntaa UTC-ajan Suomen paikallisajaksi (UTC+2 tai kesäaikana UTC+3).
// Palauttaa false, jos aikaa ei voi muuntaa.
static bool utc_to_finland_local(time_t utc, struct tm *local)
{
    struct tm utc_tm;

    if (gmtime_r(&utc, &utc_tm) == NULL)
    {
        return false;
    }

    int offset_hours = finland_dst(&utc_tm) ? 3 : 2;

    // Siirretään aikaleimaa ja puretaan se gmtime:lla: tulos on paikallinen
    // kellonaika kenttinä.
    time_t local_seconds = utc + offset_hours * 3600;

    return gmtime_r(&local_seconds, local) != NULL;
}

void ntp_clock_format_saved(
    uint32_t unix_seconds,
    char *buffer,
    size_t size)
{
    // Nolla tarkoittaa, ettei SNTP-aikaa ollut
    // saatavilla mittauksen tallennushetkellä.
    if (unix_seconds == 0)
    {
        snprintf(buffer, size, "ei aikasynkronointia");
        return;
    }

    struct tm local;

    if (!utc_to_finland_local((time_t)unix_seconds, &local))
    {
        snprintf(buffer, size, "virheellinen aika");
        return;
    }

    if (strftime(buffer, size, "%d.%m.%Y %H:%M", &local) == 0)
    {
        snprintf(buffer, size, "aikapuskurivirhe");
    }
}

void ntp_clock_format_now(char *buffer, size_t size)
{
    time_t utc;
    struct tm local;

    if (!ntp_clock_read_utc(&utc) ||
        !utc_to_finland_local(utc, &local))
    {
        snprintf(buffer, size, "--:--");
        return;
    }

    snprintf(
        buffer,
        size,
        "%02d:%02d",
        local.tm_hour,
        local.tm_min);
}
