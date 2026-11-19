#include "history_send.h"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "pico/stdlib.h"

#include "aws_client.h"
#include "measurement.h"
#include "ntp_clock.h"
#include "storage.h"

#define RETRY_DELAY_US 2000000ULL // epäonnistuneen aloituksen jälkeen: 2 s
#define NEXT_DELAY_US 200000ULL   // onnistuneen aloituksen jälkeen: 200 ms

static bool enabled = true;
static bool pending = false;       // lähetys kesken
static bool waiting_result = false; // viimeisimmän pyynnön tulosta odotetaan
static uint32_t send_index = 0;    // seuraavaksi lähetettävä mittaus
static uint64_t next_send_us = 0;

void history_send_set_enabled(bool value)
{
    enabled = value;
}

// Käynnistää koko tallennetun historian uudelleenlähetyksen AWS:ään.
// Sauna-tilassa lähetystä ei käynnistetä.
void history_send_start(void)
{
    if (!enabled)
    {
        return;
    }

    pending = true;
    send_index = 0;
    next_send_us = 0;

    // Mahdollisen keskeneräisen pyynnön tulosta ei saa lukea uuden
    // lähetyksen indeksiin, muuten mittaus 0 voitaisiin ohittaa lähettämättä.
    waiting_result = false;

    printf("Historian lahetys kaynnistetaan: %u mittausta\n",
           (unsigned)storage_count());
}

// Lähettää historian yhden mittauksen kerrallaan.
// Indeksi etenee vain, jos AWS palauttaa HTTP 2xx -vastauksen.
// Epäonnistunut lähetys yritetään uudelleen 2 sekunnin päästä.
void history_send_service(void)
{
    if (!enabled || !pending)
    {
        return;
    }

    if (aws_client_busy())
    {
        return;
    }

    time_t current_time;

    if (!ntp_clock_read_utc(&current_time))
    {
        return;
    }

    uint64_t now = time_us_64();

    if (now < next_send_us)
    {
        return;
    }

    // Edellinen AWS-pyyntö on valmistunut.
    // Siirry seuraavaan vain, jos palvelin hyväksyi sen.
    if (waiting_result)
    {
        if (aws_client_last_send_ok())
        {
            send_index++;
        }

        waiting_result = false;
    }

    // Ohitetaan mittaukset, joilla ei ollut kelvollista
    // SNTP-aikaa tallennushetkellä.
    while (send_index < storage_count() &&
           storage_get(send_index)->unix_seconds == 0)
    {
        send_index++;
    }

    if (send_index >= storage_count())
    {
        pending = false;
        printf("Historian lahetys valmis\n");
        return;
    }

    const Measurement *m = storage_get(send_index);

    bool started = aws_submit_measurement(
        (float)m->temperature_x10 / 10.0f,
        (float)m->humidity_x10 / 10.0f,
        m->unix_seconds,
        (uint32_t)current_time);

    if (started)
    {
        waiting_result = true;

        printf(
            "Historia: lahetetaan %u/%u, aika %u\n",
            (unsigned)(send_index + 1),
            (unsigned)storage_count(),
            (unsigned)m->unix_seconds);

        next_send_us = now + NEXT_DELAY_US;
    }
    else
    {
        next_send_us = now + RETRY_DELAY_US;
    }
}
