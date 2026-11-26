#include "json_pico.h"
#include "measurements.h"
#include "ntp.h"
#include "sensors_led.h"
#include "aws_send.h"
#include "aws_certs.h"
#include "tls.h"

static void blink()
{
    sleep_ms(50);
    gpio_put(LED_PIN, 1);
    sleep_ms(100);
    gpio_put(LED_PIN, 0);
}

void process_and_send_measurements(const char *dht_id, const char *ds_id, bool wifi)
{
    (void)ds_id;

    // Staattinen puskuri säästää pino-muistia (stack)
    static char json_buffer[1024];

    if (current_epoch_time <= 0)
    {
        printf("Varoitus: NTP-aikaa ei ole vielä haettu (current_epoch_time == 0).\n");
    }

    int json_len = create_json_dht(json_buffer, sizeof(json_buffer), dht_id);
    if (json_len < 0)
    {
        printf("Virhe DHT22 JSON-muodostuksessa.\n");
        return;
    }

    printf("JSON (%d tavua):\n%s\n", json_len, json_buffer);

    bool sent = false;
    if (current_epoch_time > 0 && wifi)
    {
        printf("Lähetetään DHT22 JSON AWS:ään...\n");
        sent = send_http_post(
            AWS_HOST,
            AWS_PORT,
            AWS_PATH,
            AWS_AUTHORIZATION,
            json_buffer);
    }
    else
    {
        printf("NTP-aika puuttuu tai Wi-Fi ei ole yhdistetty.\n");
    }

    if (sent)
    {
        printf("Lähetys AWS:ään onnistui!\n");
        blink();
        blink();
        measurement_count = 0;
    }
    else
    {
        printf("Lähetys epäonnistui.\n");
    }
}