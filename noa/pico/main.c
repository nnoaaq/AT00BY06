#include <stdio.h>
#include <time.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/aon_timer.h"
#include <hardware/adc.h>

#include "wifi_connect.h"

#include "ntp.h"
#include "tls.h"
#include "aws_config.h"
#include "measurement.h"

struct timespec ts; // GET_TIME PALAUTTAA AON_TIMERIN AJAN
void setup()
{
    stdio_init_all();
    sleep_ms(1500);

    wifi_connect_init(); // ALUSTAA WIFIN JA ALOITTAA YHDISTÄMISEN TAUSTALLA

    // WIFI-YHTEYS VAADITAAN KERRAN SETUPISSA, ODOTETAAN KUNNES SE TAPAHTUU
    // 15 SEK PER YRITYS / 5 SEK YRITYSTEN VÄLILLÄ
    while (wifi_connect_poll() != WIFI_CONNECTED)
    {
        sleep_ms(1000);
    }

    ntp_init(); // ALOITETAAN NTP-HAKU TAUSTALLA

    // NTP-SYNKRONOINTI VAADITAAN KERRAN SETUPISSA, ODOTETAAN KUNNES AON_TIMER ON SYNKRONOITU
    while (get_time(&ts) != 0)
    {
        sleep_ms(1000);
    }

    // NTP-AIKA SYNKRONOITU ONNISTUNEESTI, SIIRRYTÄÄN MAIN-LOOPPIIN

    // ALUSTETAAN ADC
    adc_init();          // ADC - KÄYTTÖÖN
    adc_gpio_init(26);   // GPIO26 - PINNIIN KYTKETTY MITTAUSPISTE
    adc_select_input(0); // ADC0

    // ASETETAAN AWS-CA-CERTIFIKAATTI
    tls_add_cert(aws_cert, sizeof(aws_cert));
}

int main()
{
    setup();

    // VARATAAN (buffer) 256 TAVUA TILAA MITTAUSTULOKSEN TALLENTAMISELLE
    char measurement[256];
    while (true)
    {
        // if (wifi_is_connected())
        //{
        //     if (get_measured_temperature(measurement, sizeof(measurement)))
        //     {
        //         printf("%s\n", measurement);
        //         printf("Lähetetään mittaus palvelimelle.\n\n");
        //         http_send(measurement, AWS_HOST);
        //     }
        // }
        // else
        //{
        //     printf("Ei verkkoyhteyttä\n");
        //     connect_to_wifi();
        // }

        wifi_connect_poll();

        if (wifi_is_connected())
        {
            // VERKKOYHTEYS OK
            if (get_measured_temperature(measurement, sizeof(measurement)))
            {
                // MITTAUS OK
                http_send(measurement, AWS_HOST);
            }
        }
        sleep_ms(60000 * 15);
    }
}
