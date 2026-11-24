#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"

#include "sensors_led.h"
#include "measurements.h"
#include "json_pico.h"
#include "pico_wifi_scan.h"
#include "sensor_task.h"
#include "sensors_led.h"
#include "tls.h"
#include "ntp.h"
#include "onewire_library.h"
#include "aws.h"

#define MEASUREMENT_INTERVAL_MS 60000
#define WIFI_SCAN_INTERVAL_MS 10000

void setup_gpio(void)
{
    stdio_init_all();
    sleep_ms(3000);

    gpio_init(DHT_PIN);
    gpio_set_dir(DHT_PIN, GPIO_IN);
    gpio_pull_up(DHT_PIN);

    gpio_init(DS18B20_PIN);
    gpio_set_dir(DS18B20_PIN, GPIO_IN);
    gpio_pull_up(DS18B20_PIN);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    printf("GPIO-alustus valmis\n");
}

int main(void)
{
    setup_gpio();

    NTP_T *ntp_state = NULL;
    bool ntp_started = false;
    OW ow;
    if (!setup_onewire(&ow, pio0))
    {
        while (true)
        {
            sleep_ms(1000);
        } // Pysäytetään virhetilanteessa
    }

    if (cyw43_arch_init())
    {
        printf("Virhe: CYW43-alustus epäonnistui\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    // Wi-Fi-skannauksen ajastin
    wifi_scan_state_t scan_state = {.scan_started = false, .network_found = false};
    bool wifi_connected = false;

    async_at_time_worker_t scan_worker = {
        .do_work = scan_worker_fn,
        .user_data = &scan_state};
    hard_assert(async_context_add_at_time_worker_in_ms(cyw43_arch_async_context(), &scan_worker, 0));

    const char dht_id[] = "dht_JN";
    const char ds_id[] = "ds_1_JN";

    // Ajastin anturimittauksille (ensimmäinen mittaus heti)
    absolute_time_t next_measurement_time = get_absolute_time();
    bool running = true;

    while (running)
    {
        // 1. Pidä Wi-Fi-piiri ja taustatehtävät aktiivisina
        cyw43_arch_poll();

        // 2. Tarkista lopetuskomento
        int key = getchar_timeout_us(0);
        if (key == 'q' || key == 'Q')
        {
            running = false;
        }

        // 3. Skannauksen tilan tarkistus
        if (!cyw43_wifi_scan_active(&cyw43_state) && scan_state.scan_started)
        {
            scan_state.scan_started = false;

            if (scan_state.network_found)
            {
                printf("Skannaus lopetettu. Yhdistetään verkkoon...");
                // Yhdistetään verkkoon (30 sekunnin aikakatkaisu)
                int status = cyw43_arch_wifi_connect_timeout_ms(TARGET_SSID, TARGET_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000);

                if (status == 0)
                {
                    printf("Yhdistetty Wi-Fi-verkkoon!\n");
                    wifi_connected = true;
                    if (!ntp_started)
                    {
                        ntp_state = ntp_init();
                        if (ntp_state)
                        {
                            ntp_start(ntp_state);
                            ntp_started = true;
                        }
                    }
                }
                else
                {
                    printf("Yhdistäminen epäonnistui (virhekoodi %d). Yritetään skannausta uudelleen.\n", status);
                    scan_state.network_found = false;
                    async_context_add_at_time_worker_in_ms(cyw43_arch_async_context(), &scan_worker, WIFI_SCAN_INTERVAL_MS);
                }
            }
            else
            {
                printf("Verkkoa ei löytynyt, skannataan uudelleen %d ms kuluttua.\n", WIFI_SCAN_INTERVAL_MS);
                async_context_add_at_time_worker_in_ms(cyw43_arch_async_context(), &scan_worker, WIFI_SCAN_INTERVAL_MS);
            }
        }

        // Anturimittaus 60 sekunnin välein ilman sleep
        if (current_epoch_time > 0 && time_reached(next_measurement_time))
        {
            run_sensor_measurement_cycle(&ow);

            if (measurement_count >= MAX_MEASUREMENTS)
            {
                if (wifi_connected)
                {
                    process_and_send_measurements(dht_id, ds_id, wifi_connected);
                }
                else
                {
                    printf("Ei verkkoyhteyttä, mittauksia ei voida vielä lähettää.\n");
                }
            }

            // Asetetaan seuraava mittausaika 60 sekunnin päähän
            next_measurement_time = make_timeout_time_ms(MEASUREMENT_INTERVAL_MS);
        }

        // Lyhyt lepo säästää virtaa, mutta ei haittaa Wi-Fi-pollausta
        sleep_ms(10);
    }

    // Siivotaan resurssit kun silmukasta poistutaan ('q')
    printf("Suljetaan järjestelmä...\n");
    ntp_deinit(ntp_state);
    async_context_remove_at_time_worker(cyw43_arch_async_context(), &scan_worker);
    cyw43_arch_disable_sta_mode();
    cyw43_arch_deinit();

    return 0;
}