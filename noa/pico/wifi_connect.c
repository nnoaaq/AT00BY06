#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "wifi_config.h"
#include "wifi_connect.h"

// MÄÄRITELLÄÄN AIKARAJAT AIKAKATKAISULLE SEKÄ UUDELLEEN YRITYKSEN ODOTUKSELLE
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define WIFI_RETRY_DELAY_MS 5000

// MUUTTUJIEN MÄÄRITTELY
static wifi_state_t wifi_state = WIFI_IDLE;
static absolute_time_t wifi_timeout;
static absolute_time_t wifi_retry;

// WIFIN ALUSTUS-FUNKTIO
// FALSE > ALUSTUS EPÄONNISTUI, TRUE > ALUSTUS ONNISTUI

bool wifi_connect_start()
{
    printf("WiFi: aloitetaan yhteyden muodostaminen verkkoon %s\n", WIFI_SSID);
    int err = cyw43_arch_wifi_connect_async(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK);
    if (err)
    {
        wifi_state = WIFI_FAILED;
        wifi_retry = make_timeout_time_ms(WIFI_RETRY_DELAY_MS); // LUODAAN AIKALEIMA X-(ms)SEKUNTIEN PÄÄHÄN UUDELLEENYRITYSTÄ VARTEN
        printf("WiFi: käynnistys epäonnistui\n");
        return false;
    }
    wifi_timeout = make_timeout_time_ms(WIFI_CONNECT_TIMEOUT_MS);
    wifi_state = WIFI_CONNECTING;
    return true;
}
bool wifi_connect_init()
{
    if (cyw43_arch_init())
    {
        return false;
    }
    cyw43_arch_enable_sta_mode();
    wifi_connect_start();
    return true;
}
bool wifi_is_connected()
{
    return wifi_state == WIFI_CONNECTED;
}

wifi_state_t wifi_connect_poll()
{
    switch (wifi_state)
    {
    case WIFI_CONNECTING:
    {
        int status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
        /*
        CYW43_LINK_DOWN    (0)  >> link is down
        CYW43_LINK_JOIN    (1)  >> Connected to wifi
        CYW43_LINK_NOIP    (2)  >> Connected to wifi, but no IP address
        CYW43_LINK_UP      (3)  >> Connected to wifi with an IP address
        CYW43_LINK_FAIL    (-1) >> Connection failed
        CYW43_LINK_NONET   (-2) >> No matching SSID found (could be out of range, or down)
        CYW43_LINK_BADAUTH (-3) >> Authenticatation failure
        */
        if (status == CYW43_LINK_UP)
        {
            wifi_state = WIFI_CONNECTED;
            printf("WiFi: yhteys muodostettu. (%s)\n", ipaddr_ntoa(netif_ip4_addr(netif_default)));
        }
        else if (status == CYW43_LINK_FAIL || status == CYW43_LINK_NONET || status == CYW43_LINK_BADAUTH)
        {
            wifi_state = WIFI_FAILED;
            wifi_retry = make_timeout_time_ms(WIFI_RETRY_DELAY_MS);
            printf("WiFi: virhe (%d)\n", status);
        }
        else if (time_reached(wifi_timeout))
        {
            wifi_state = WIFI_FAILED;
            wifi_retry = make_timeout_time_ms(WIFI_RETRY_DELAY_MS);
            printf("WiFi: virhe (aikakatkaisu)\n");
        }
        break; // CASEN LOPPU
    }
    case WIFI_CONNECTED:
    {
        int status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
        if (status != CYW43_LINK_UP)
        {
            wifi_state = WIFI_FAILED;
            wifi_retry = make_timeout_time_ms(WIFI_RETRY_DELAY_MS);
            printf("WiFi: yhteys katkennut\n");
        }
        break;
    }
    case WIFI_FAILED:
    {
        if (time_reached(wifi_retry))
        {
            wifi_connect_start();
        }
    }
    case WIFI_IDLE:
    {
        break;
    }
    defaul:
        {
            break;
        }
    }
    return wifi_state;
}