#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/apps/sntp.h"
#include <hardware/adc.h>
#include "wifi.h"
#include "ntp.h"
#include "tls.h"
#include "measurement.h"
int main()
{
    stdio_init_all();

    // WIFI ALKAA
    if (cyw43_arch_init())
    {
        printf("WiFi init epäonnistui.\n");
        return -1;
    }
    cyw43_arch_enable_sta_mode();
    bool wifi_connected = connect_to_wifi();
    if (!wifi_connected)
    {
        printf("Verkkoyhteyttä ei pystytty muodostamaan.\n");
        while (true)
        {
            sleep_ms(5000);
        }
    }
    // WIFI LOPPUU

    // NTP ALKAA
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_init();
    setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1); // POSIX
    struct timespec ts;                              // GET_TIME PALAUTTAA AON_TIMERIN AJAN
    // get_time = 0 TARKOITTAA OK, 1 TARKOITTAA EI OK
    while (get_time(&ts) != 0)
    {
        // EI EDETÄ ENNENKUIN AIKA ON SYNKRYNOITU!!!
        printf(".");
        sleep_ms(1000);
    }
    // NTP LOPPUU

    // MITTAUS ALKAA
    adc_init();          // ADC - KÄYTTÖÖN
    adc_gpio_init(26);   // GPIO26 - PINNIIN KYTKETTY MITTAUSPISTE
    adc_select_input(0); // ADC0
    char measurement_buffer[256];
    // MITTAUS LOPPUU

    while (true)
    {
        get_time(&ts);
        printf("Picon kello: %lld\n", ts.tv_sec);
        if (get_measured_temperature(measurement_buffer, sizeof(measurement_buffer)))
        {
            printf("%s\n", measurement_buffer);
            printf("Lähetetään mittaus palvelimelle.\n\n");
            http_init(measurement_buffer);
        }
        sleep_ms(60000 * 15);
    }
}
