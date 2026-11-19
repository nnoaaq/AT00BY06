#include "network.h"

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/ip_addr.h"
#include "lwip/apps/sntp.h"

#include "wifi_settings.h"

#define CONNECT_TIMEOUT_US 20000000ULL // yhdistämisyritykselle annettu aika
#define RETRY_DELAY_US 30000000ULL     // odotus epäonnistuneen yrityksen jälkeen

static bool initialized = false;
static bool paused = false;
static bool online = false;
static bool connecting = false;
static bool services_started = false; // HTTP-palvelin käynnistetty
static uint64_t connect_deadline_us = 0;
static uint64_t retry_at_us = 0;

static HttpStatusFn http_status_fn = NULL;
static NetworkConnectedFn connected_fn = NULL;

bool network_init(HttpStatusFn status_fn, NetworkConnectedFn on_connected)
{
    if (cyw43_arch_init() != 0)
    {
        return false;
    }

    cyw43_arch_enable_sta_mode();

    http_status_fn = status_fn;
    connected_fn = on_connected;
    retry_at_us = 0;
    initialized = true;

    return true;
}

void network_set_paused(bool value)
{
    if (value == paused)
    {
        return;
    }

    paused = value;

    if (value && initialized)
    {
        cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
        online = false;
        connecting = false;
    }
}

// Hallitsee Wi-Fi-yhteyden tilan.
// Yhdistyksen jälkeen käynnistää SNTP:n ja HTTP-palvelimen ja ilmoittaa
// yhteyden muodostumisesta.
void network_service(void)
{
    if (!initialized)
    {
        return;
    }

    // Wi-Fi-ajuria pollataan myös pysäytettynä.
    cyw43_arch_poll();

    if (paused)
    {
        return;
    }

    uint64_t now = time_us_64();
    int status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);

    if (status == CYW43_LINK_UP)
    {
        if (!online)
        {
            printf("Wi-Fi yhdistetty. IP: %s\n",
                   ipaddr_ntoa(&cyw43_state.netif[CYW43_ITF_STA].ip_addr));

            // Käynnistetään SNTP aina uudelleen, jotta kello synkronoituu
            // myös katkon jälkeen.
            sntp_stop();
            sntp_setoperatingmode(SNTP_OPMODE_POLL);
            sntp_setservername(0, "pool.ntp.org");
            sntp_init();

            if (connected_fn)
            {
                connected_fn();
            }
        }

        online = true;
        connecting = false;

        // HTTP-palvelin käynnistetään kerran. Epäonnistuessa yritetään
        // uudelleen 30 sekunnin välein.
        if (!services_started && now >= retry_at_us)
        {
            services_started = http_server_start(http_status_fn);
            retry_at_us = now + RETRY_DELAY_US;
        }

        return;
    }

    if (online)
    {
        printf("Wi-Fi katkesi. Mittaus jatkuu paikallisesti.\n");
        online = false;
        retry_at_us = now;
    }

    if (connecting)
    {
        // Annetaan asynkronisen yrityksen käyttää aikansa loppuun, vaikka
        // tila olisi jo NONET.
        if (now >= connect_deadline_us)
        {
            printf("Wi-Fi-yritys paattyi (tila %d). Uusi yritys 30 s kuluttua.\n", status);
            cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
            connecting = false;
            retry_at_us = now + RETRY_DELAY_US;
        }

        return;
    }

    if (now < retry_at_us)
    {
        return;
    }

    printf("Aloitetaan Wi-Fi-yhteysyritys.\n");

    int result = cyw43_arch_wifi_connect_async(
        WIFI_SSID,
        WIFI_PASSWORD,
        CYW43_AUTH_WPA2_AES_PSK);

    if (result == 0)
    {
        connecting = true;
        connect_deadline_us = now + CONNECT_TIMEOUT_US;
    }
    else
    {
        printf("Wi-Fi-yrityksen kaynnistys epaonnistui: %d\n", result);
        retry_at_us = now + RETRY_DELAY_US;
    }
}
