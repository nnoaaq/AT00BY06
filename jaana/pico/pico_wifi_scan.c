
#include <stdio.h>
#include <string.h>
#include "pico_wifi_scan.h"

static int scan_result(void *env, const cyw43_ev_scan_result_t *result)
{
    if (result)
    {
        wifi_scan_state_t *state = (wifi_scan_state_t *)env;

        // Tarkistetaan vastaako löydetty verkko omaa verkkoa
        if (strncmp((const char *)result->ssid, TARGET_SSID, result->ssid_len) == 0 &&
            strlen(TARGET_SSID) == result->ssid_len)
        {
            printf("Haluttu verkko löydetty.\n");
            state->network_found = true;
            // Palauttamalla muun kuin 0, CYW43-ajuri lopettaa meneillään olevan skannauksen heti
            return 1;
        }
    }
    return 0;
}

void scan_worker_fn(async_context_t *context, async_at_time_worker_t *worker)
{
    wifi_scan_state_t *state = (wifi_scan_state_t *)worker->user_data;
    if (state->network_found)
        return; // Ei aloiteta enää uutta

    cyw43_wifi_scan_options_t scan_options = {0};
    // Välitetään 'state' kolmantena parametrina (env), jotta scan_result pääsee siihen käsiksi
    int err = cyw43_wifi_scan(&cyw43_state, &scan_options, state, scan_result);
    if (err == 0)
    {
        state->scan_started = true;
        printf("Suoritetaan Wi-Fi-skannaus...\n");
    }
    else
    {
        printf("Skannauksen aloitus epäonnistui: %d\n", err);
    }
}
