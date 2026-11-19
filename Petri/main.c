// Pico 2 W -sääasema
//
// Pääohjelma alustaa laitteet ja pyörittää pääsilmukkaa (poll-arkkitehtuuri,
// ei keskeytyksiä eikä toista ydintä). Silmukka
//   - ajaa verkon, AWS-lähetyksen ja historian jälkilähetyksen,
//   - lukee anturin 10 sekunnin välein,
//   - päivittää LCD:n ja minuuttikeskiarvon minuutin välein,
//   - tallentaa keskiarvon flashiin valitun tallennusjakson päättyessä.
//
// Muut tehtävät on jaettu moduuleihin:
//   lcd, dht22, adc_sensors   laitteisto
//   storage, measurement      flash-historia
//   ntp_clock                 SNTP-kello ja Suomen aika
//   network, http_server      Wi-Fi ja tilasivu
//   aws_client, history_send  mittausten lähetys AWS:ään

#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/gpio.h"

#include "adc_sensors.h"
#include "average.h"
#include "aws_client.h"
#include "config.h"
#include "dht22.h"
#include "history_send.h"
#include "http_server.h"
#include "lcd.h"
#include "measurement.h"
#include "network.h"
#include "ntp_clock.h"
#include "storage.h"

// ---------------- Tila ----------------

// Tallennusjakso ja sauna-tila.
// Sauna-tilassa tallennetaan minuutin välein ja Wi-Fi sekä AWS ovat pois.
static unsigned save_interval_minutes = DEFAULT_SAVE_MINUTES;
static uint64_t next_storage_us = 0;
static bool sauna_mode = false;

// Minuuttilaskuri. last_finished_minute on viimeksi päättyneen minuutin numero.
static uint32_t minute_counter = 1;
static uint32_t last_finished_minute = 0;

// Keskiarvoikkunat: näytölle (1 min) ja tallennukseen (tallennusjakso).
static Average display_average;
static Average storage_average;

static uint64_t next_sample_us = 0;
static uint64_t next_display_us = 0;

// Viimeisin minuuttikeskiarvo (LCD ja tilasivu).
static bool latest_valid = false;
static float latest_temperature = 0.0f;
static float latest_humidity = 0.0f;
static float latest_internal = 0.0f;
static uint8_t latest_battery = 0;
static char latest_time[16] = "--:--";

// ---------------- Tilasivun tiedot ----------------

// HTTP-palvelin kutsuu tätä jokaisella sivulatauksella.
static void fill_http_status(HttpStatus *status)
{
    uint64_t now = time_us_64();

    status->measurement_valid = latest_valid;
    status->temperature = latest_temperature;
    status->humidity = latest_humidity;
    status->internal_temperature = latest_internal;
    status->battery_percent = latest_battery;
    snprintf(status->measurement_time, sizeof(status->measurement_time),
             "%s", latest_time);
    status->stored_count = storage_count();
    status->interval_minutes = save_interval_minutes;

    // Jäljellä oleva aika pyöristetään ylöspäin sekunteihin.
    status->seconds_to_period_end =
        next_storage_us > now
            ? (unsigned)((next_storage_us - now + 999999ULL) / 1000000ULL)
            : 0;
}

// ---------------- USB-monitori ----------------

// Tulosta historia uudelleen USB-monitorin avautuessa.
// Ei odota USB:ta patterikäytössä eikä muuta tallennuksia.
static void service_usb_history(void)
{
    static bool was_connected = false;
    static bool pending = false;
    static uint64_t print_after_us = 0;

    bool connected = stdio_usb_connected();

    if (!connected)
    {
        was_connected = false;
        pending = false;
        return;
    }

    // Monitorin avautuessa odotetaan sekunti, jotta terminaali ehtii
    // valmiiksi ennen tulostusta.
    if (!was_connected)
    {
        was_connected = true;
        pending = true;
        print_after_us = time_us_64() + 1000000ULL;
    }

    if (pending && time_us_64() >= print_after_us)
    {
        pending = false;
        storage_print_history();
    }
}

// ---------------- Tallennusvälin vaihto ----------------

// Ottaa käyttöön selaimesta pyydetyn tallennusvälin (1, 15 tai 60 min).
// Uusi jakso alkaa heti, ja keskeneräinen keskiarvo hylätään.
static void apply_interval_request(unsigned requested, uint64_t now)
{
    // Sauna-tilassa Wi-Fi ja AWS on tarkoituksella pois käytöstä.
    // Mittaukset tallentuvat silti paikallisesti.
    if (requested == 1)
    {
        if (sauna_mode)
        {
            printf("Sauna-tila on jo kaynnissa.\n");
            return;
        }

        sauna_mode = true;
        save_interval_minutes = 1;
        average_reset(&storage_average);
        next_storage_us = now + (uint64_t)save_interval_minutes * DISPLAY_INTERVAL_US;

        printf("Sauna-tila kaynnistetty: 1 min tallennus, Wi-Fi pois, AWS pois.\n");

        history_send_set_enabled(false);
        network_set_paused(true);
    }
    else if (requested == 15 || requested == 60)
    {
        if (!sauna_mode && requested == save_interval_minutes)
        {
            printf("Tallennusvali on jo %u min; jakso jatkuu ennallaan.\n", requested);
            return;
        }

        sauna_mode = false;
        save_interval_minutes = requested;
        average_reset(&storage_average);
        next_storage_us = now + (uint64_t)requested * DISPLAY_INTERVAL_US;

        printf("Uusi tallennusvali %u min. Wi-Fi palaa kayttoon.\n", requested);

        history_send_set_enabled(true);
        network_set_paused(false);
    }
}

// ---------------- Mittaus, näyttö ja tallennus ----------------

// Lukee anturin ja lisää kelvollisen mittauksen keskiarvoihin.
static void take_sample(uint64_t now)
{
    // Seuraava näyte; jos silmukka on myöhästynyt, ohitetaan väliin
    // jääneet ajankohdat eikä yritetä korvata niitä.
    do
    {
        next_sample_us += SAMPLE_INTERVAL_US;
    } while (next_sample_us <= now);

    float temperature;
    float humidity;

    if (!dht22_read(&temperature, &humidity))
    {
        printf("DHT22-luku epaonnistui\n");
        return;
    }

    // Anturin ilmoittamien rajojen ulkopuoliset arvot ovat lukuvirheitä.
    bool valid = isfinite(temperature) && isfinite(humidity) &&
                 temperature >= -40.0f && temperature <= 80.0f &&
                 humidity >= 0.0f && humidity <= 100.0f;

    if (!valid)
    {
        printf("Anturilta tuli epakelpo mittaus\n");
        return;
    }

    temperature += TEMP_CORRECTION_C;
    humidity += HUMIDITY_CORRECTION;

    if (humidity < 0.0f)
    {
        humidity = 0.0f;
    }

    if (humidity > 100.0f)
    {
        humidity = 100.0f;
    }

    float internal = adc_read_internal_temperature();

    average_add(&display_average, temperature, humidity, internal);
    average_add(&storage_average, temperature, humidity, internal);

    printf("Nayte: %.1f C, %.1f %%, Pico %.1f C\n",
           (double)temperature, (double)humidity, (double)internal);
}

// Päättää minuutin: päivittää viimeisimmät arvot ja LCD:n minuuttikeskiarvosta.
static void finish_display_minute(uint64_t now)
{
    uint32_t elapsed = 0;

    do
    {
        next_display_us += DISPLAY_INTERVAL_US;
        elapsed++;
    } while (next_display_us <= now);

    last_finished_minute = minute_counter + elapsed - 1;

    if (display_average.count)
    {
        latest_temperature = (float)(display_average.temperature / display_average.count);
        latest_humidity = (float)(display_average.humidity / display_average.count);
        latest_internal = (float)(display_average.internal / display_average.count);
        latest_battery = adc_read_battery_percent();
        latest_valid = true;

        ntp_clock_format_now(latest_time, sizeof(latest_time));

        lcd_show_measurement(
            latest_temperature,
            latest_humidity,
            latest_battery,
            latest_internal);

        printf("Minuutti %" PRIu32 " | Klo %s | Ulko %.1f C | Kosteus %.1f %%"
               " | Pico %.1f C | Akku %u %% | Naytteita %" PRIu32 "\n",
               last_finished_minute, latest_time, (double)latest_temperature,
               (double)latest_humidity, (double)latest_internal,
               (unsigned)latest_battery, display_average.count);
    }
    else
    {
        latest_valid = false;
        lcd_write_line(0, "Anturivirhe");
        lcd_write_line(1, "Ei mittauksia");
        printf("Minuutti %" PRIu32 ": ei onnistuneita mittauksia\n", last_finished_minute);
    }

    if (elapsed > 1)
    {
        printf("Varoitus: nayttopaivityksia myohastyi; puuttuvia naytteita ei keksita.\n");
    }

    average_reset(&display_average);
    minute_counter += elapsed;
}

// Tallentaa valmiin keskiarvon flash-muistiin.
// Jos NTP-aikaa ei ole, tallennetaan unix_seconds = 0.
// Tällaisia mittauksia ei lähetetä AWS:ään.
static void store_average(const Average *a, uint32_t ending_minute)
{
    if (!a->count)
    {
        printf("Tallennusjakso %u min: ei onnistuneita naytteita, ei tallennusta.\n",
               save_interval_minutes);
        return;
    }

    time_t utc = 0;
    uint32_t stamp = ntp_clock_read_utc(&utc) ? (uint32_t)utc : 0;

    double temperature = a->temperature / a->count;
    double humidity = a->humidity / a->count;
    double internal = a->internal / a->count;

    Measurement incoming = {
        .minute = ending_minute,
        .unix_seconds = stamp,
        .temperature_x10 = (int16_t)lround(temperature * 10.0),
        .humidity_x10 = (uint16_t)lround(humidity * 10.0),
        .internal_temperature_x10 = (int16_t)lround(internal * 10.0),
        .battery_percent = adc_read_battery_percent(),
        .interval_minutes = (uint8_t)save_interval_minutes};

    if (storage_append(&incoming))
    {
        printf("Tallennettu: %" PRIu32 "/%u (uusimmat mittaukset)\n",
               storage_count(), (unsigned)MAX_MEASUREMENTS);
    }
    else
    {
        printf("Tallennus epaonnistui; historia ennallaan.\n");
    }

    printf("Tallennusjakso %u min: %.1f C, %.1f %%, naytteita %" PRIu32
           ", aikaleima %" PRIu32 "\n",
           save_interval_minutes, temperature, humidity, a->count, stamp);

    // Uusi mittaus lähetetään AWS:ään vain, jos sillä on kelvollinen aika.
    if (stamp)
    {
        history_send_start();
    }
}

// Päättää tallennusjakson: tallentaa keskiarvon ja aloittaa uuden jakson.
static void finish_storage_period(uint64_t now)
{
    uint64_t period = (uint64_t)save_interval_minutes * DISPLAY_INTERVAL_US;
    unsigned elapsed = 0;

    do
    {
        next_storage_us += period;
        elapsed++;
    } while (next_storage_us <= now);

    store_average(&storage_average, last_finished_minute);
    average_reset(&storage_average);

    if (elapsed > 1)
    {
        printf("Varoitus: tallennusjaksoja myohastyi; tallennettiin vain olemassa olevat naytteet.\n");
    }
}

// ---------------- Pääohjelma ----------------

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);

    if (lcd_init())
    {
        lcd_write_line(0, "Pico saa-asema");
        lcd_write_line(1, "Kaynnistyy...");
    }
    else
    {
        printf("LCD ei vastaa osoitteessa 0x%02X\n", LCD_ADDR);
    }

    adc_sensors_init();
    dht22_init();

    gpio_init(RESET_BUTTON_PIN);
    gpio_set_dir(RESET_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(RESET_BUTTON_PIN);

    sleep_ms(10);

    // Nappi pohjassa käynnistyksessä tyhjentää tallennetun historian.
    if (!gpio_get(RESET_BUTTON_PIN))
    {
        printf("Tyhjennetaan mittausmuisti\n");

        if (storage_erase())
        {
            lcd_write_line(0, "Muisti");
            lcd_write_line(1, "tyhjennetty");
        }
        else
        {
            lcd_write_line(0, "Flash-virhe");
            lcd_write_line(1, "Katso sarjaportti");
        }

        sleep_ms(1000);
    }

    storage_load();
    minute_counter = storage_next_minute();
    last_finished_minute = minute_counter - 1;

    bool network_ok = network_init(fill_http_status, history_send_start);

    if (!network_ok)
    {
        printf(
            "Wi-Fi-alustus epaonnistui. "
            "Jatketaan ilman verkkoa.\n");
    }

    lcd_write_line(0, "Odotetaan");
    lcd_write_line(1, "mittauksia...");

    uint64_t started = time_us_64();
    next_sample_us = started + SAMPLE_INTERVAL_US;
    next_display_us = started + DISPLAY_INTERVAL_US;
    next_storage_us = started + (uint64_t)save_interval_minutes * DISPLAY_INTERVAL_US;

    printf("Tallennusvali: %u min. LCD: 1 min. Naytevali: 10 s.\n", save_interval_minutes);

    while (true)
    {
        if (network_ok)
        {
            network_service();
            aws_client_poll();
            history_send_service();
        }

        service_usb_history();

        uint64_t now = time_us_64();

        unsigned requested = http_server_take_interval_request();

        if (requested)
        {
            apply_interval_request(requested, now);
        }

        if (now >= next_sample_us)
        {
            take_sample(now);
        }

        // Näytteenotto (DHT22) kestää useita millisekunteja, joten
        // kellonaika luetaan uudelleen.
        now = time_us_64();

        if (now >= next_display_us)
        {
            finish_display_minute(now);
        }

        if (now >= next_storage_us)
        {
            finish_storage_period(now);
        }

        sleep_ms(1);
    }
}
