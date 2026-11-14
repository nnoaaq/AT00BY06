#include <stdio.h>
#include <stdbool.h>
#include "pico/cyw43_arch.h"

// WIFI TIEDOT MÄÄRITELTY wifi_config.h
#include "wifi_config.h"

#define MAX_RETRIES 3

bool connect_to_wifi()
{
    // PALAUTTAA TRUE JOS WIFIIN YHTEYS
    // PALAUTTAA FALSE JOS WIFIIN EI YHTEYTTÄ
    for (int i = 1; i < MAX_RETRIES; i++)
    {
        int err = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 30000);
        if (err == 0)
        {
            printf("Verkkoyhteys muodostettu.\n");
            uint8_t *ip_address = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
            printf("Saatu IP-osoite: %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);

            return true;
        }
        printf("Virhe verkkoyhteyden kanssa. (%d).\n", err);
        sleep_ms(2000);
    }
    return false;
}