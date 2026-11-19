#include <stdio.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/altcp_tls.h"
#include "lwip/dns.h"
#include "lwip/pbuf.h"

// Käytetään mbedTLS-kirjastoa TLS-yhteyteen.
// CA-varmenne ladataan aws_ca.h-tiedostosta.
#include "mbedtls/ssl.h"
#include "mbedtls/platform_time.h"

#include "aws_client.h"
#include "aws_settings.h"
#include "aws_ca.h"

// mbedTLS käyttää tätä aikakatkaisujen ja istuntojen ajastukseen.
// Aika on Picon käynnistymisestä kulunut aika millisekunteina.
mbedtls_ms_time_t mbedtls_ms_time(void)
{
    return (mbedtls_ms_time_t)(time_us_64() / 1000ULL);
}

// Vain poll-arkkitehtuuria varten:
// kaikkia julkisia funktioita kutsutaan pääsilmukasta.
//
// Tila on yhden pyynnön kokoinen: pyyntö (request), vastaus (response) ja
// yhteyden tilalippuja. Uusi pyyntö voidaan aloittaa vasta, kun edellinen
// on päättynyt (busy == false).
static struct
{
    struct altcp_tls_config *config;
    struct altcp_pcb *pcb;

    bool busy;
    bool dns_pending;
    bool dns_expired;

    uint64_t deadline;

    uint32_t clock_base;
    uint64_t clock_us;

    char request[1100];
    char response[1536];

    size_t request_len;
    size_t received;

    int http_status;
    bool headers_complete;
    bool finish;
    bool last_send_ok;
} s;

// TLS-varmenteiden tarkistus vaatii oikean ajan.
// Pico ei tiedä aikaa ilman SNTP:tä, joten aika annetaan erikseen.
static time_t tls_time(time_t *out)
{
    time_t now = (time_t)((uint64_t)s.clock_base +
                          (time_us_64() - s.clock_us) / 1000000ULL);

    if (out)
    {
        *out = now;
    }

    return now;
}

// Sulkee TLS/TCP-yhteyden ja nollaa lähetystilan.
static void drop_connection(void)
{
    struct altcp_pcb *pcb = s.pcb;

    s.pcb = NULL;

    if (pcb)
    {
        altcp_arg(pcb, NULL);
        altcp_err(pcb, NULL);
        altcp_recv(pcb, NULL);
        altcp_sent(pcb, NULL);
        altcp_abort(pcb);
    }

    s.busy = false;
    s.finish = false;
}

// Käsittelee verkkovirheen TLS-yhteydessä.
// Merkitsee lähetyksen epäonnistuneeksi.
static void network_error(void *arg, err_t err)
{
    (void)arg;

    // lwIP on jo vapauttanut PCB:n, joten sitä ei saa vapauttaa uudelleen.
    s.pcb = NULL;
    s.finish = true;

    printf(
        "AWS: verkko/TLS-virhe %d; tallennusta ei vahvistettu.\n",
        err);
}

// Lukee AWS:n vastauksen.
// HTTP 2xx tarkoittaa, että mittaus hyväksyttiin.
static err_t received(
    void *arg,
    struct altcp_pcb *pcb,
    struct pbuf *p,
    err_t err)
{
    (void)arg;

    if (!p)
    {
        if (!s.finish)
        {
            printf(
                "AWS: yhteys sulkeutui ilman lopullista vahvistusta.\n");
        }

        s.finish = true;
        return ERR_OK;
    }

    // Vastaus on jo käsitelty (onnistui tai epäonnistui): kuitataan loput
    // data vastaanotetuksi, mutta sitä ei tulkita enää. Ilman tätä runko
    // voisi täyttää puskurin ja aiheuttaa harhaanjohtavan virheilmoituksen
    // onnistuneen vastauksen jälkeen.
    if (s.finish)
    {
        if (err == ERR_OK)
        {
            altcp_recved(pcb, p->tot_len);
        }

        pbuf_free(p);
        return ERR_OK;
    }

    if (err == ERR_OK)
    {
        size_t room = sizeof(s.response) - 1 - s.received;
        size_t n = p->tot_len < room ? p->tot_len : room;

        s.received += pbuf_copy_partial(
            p,
            s.response + s.received,
            n,
            0);

        s.response[s.received] = 0;

        altcp_recved(pcb, p->tot_len);

        char *end = strstr(s.response, "\r\n\r\n");

        if (end && !s.headers_complete)
        {
            s.headers_complete = true;

            if (sscanf(
                    s.response,
                    "HTTP/%*u.%*u %d",
                    &s.http_status) != 1)
            {
                s.http_status = 0;
            }

            if (s.http_status >= 200 && s.http_status < 300)
            {
                s.last_send_ok = true;

                printf(
                    "AWS: palvelin hyvaksyi mittauksen, HTTP %d.\n",
                    s.http_status);

                s.finish = true;
            }
            else
            {
                printf(
                    "AWS: HTTP %d, vastaus (enintaan 1535 tavua):\n%s\n",
                    s.http_status,
                    s.response);

                s.finish = true;
            }
        }

        // Vastausotsakkeet eivät mahtuneet puskuriin kokonaan.
        if (!s.headers_complete && n < p->tot_len)
        {
            printf("AWS: vastausotsakkeet liian suuret.\n");
            s.finish = true;
        }
    }
    else
    {
        printf("AWS: vastaanottovirhe %d.\n", err);
        s.finish = true;
    }

    pbuf_free(p);
    return ERR_OK;
}

// Lähettää HTTP POST -pyynnön, kun TLS-yhteys on valmis.
static err_t connected(
    void *arg,
    struct altcp_pcb *pcb,
    err_t err)
{
    (void)arg;

    if (err != ERR_OK)
    {
        printf("AWS: yhdistaminen epaonnistui %d.\n", err);
        s.finish = true;
        return ERR_OK;
    }

    err = altcp_write(
        pcb,
        s.request,
        (u16_t)s.request_len,
        TCP_WRITE_FLAG_COPY);

    if (err == ERR_OK)
    {
        err = altcp_output(pcb);
    }

    if (err != ERR_OK)
    {
        printf("AWS: lahetysvirhe %d.\n", err);
        s.finish = true;
    }

    return ERR_OK;
}

// Luo TLS-yhteyden AWS:ään porttiin 443.
// Käyttää CA-varmennetta palvelimen tarkistamiseen.
static void connect_ip(const ip_addr_t *ip)
{
    s.pcb = altcp_tls_new(s.config, IPADDR_TYPE_V4);

    if (!s.pcb)
    {
        printf("AWS: TLS-muistin varaus epaonnistui.\n");
        s.finish = true;
        return;
    }

    altcp_arg(s.pcb, NULL);
    altcp_err(s.pcb, network_error);
    altcp_recv(s.pcb, received);

    mbedtls_ssl_context *ssl = altcp_tls_context(s.pcb);

    if (mbedtls_ssl_set_hostname(ssl, AWS_HOST) != 0)
    {
        s.finish = true;
        return;
    }

    err_t err = altcp_connect(s.pcb, ip, 443, connected);

    if (err != ERR_OK)
    {
        printf("AWS: yhteyden aloitusvirhe %d.\n", err);
        s.finish = true;
    }
}

// DNS-haun tulos. Kutsutaan, kun palvelimen osoite on selvinnyt tai haku
// on epäonnistunut. Jos pyyntö ehti jo aikakatkaistua (dns_expired),
// tulosta ei käytetä, vaan pyyntö päätetään.
static void dns_found(
    const char *name,
    const ip_addr_t *ip,
    void *arg)
{
    (void)name;
    (void)arg;

    s.dns_pending = false;

    if (s.dns_expired)
    {
        s.dns_expired = false;
        s.finish = true;
        return;
    }

    if (!ip)
    {
        printf("AWS: DNS-haku epaonnistui.\n");
        s.finish = true;
        return;
    }

    connect_ip(ip);
}

// Sallitaan asetuksissa vain merkit, jotka eivät vaadi JSON- tai
// otsake-escapea (ei ohjausmerkkejä, lainausmerkkiä eikä kenoviivaa).
static bool safe_setting(const char *v)
{
    for (; *v; ++v)
    {
        unsigned char c = (unsigned char)*v;

        if (c < 32 || c == 127 || c == '"' || c == '\\')
        {
            return false;
        }
    }

    return true;
}

// Palauttaa true, kun lähetys on käynnissä. Uutta mittausta ei voi
// aloittaa ennen kuin tämä palauttaa false.
bool aws_client_busy(void)
{
    return s.busy;
}

// Palauttaa tiedon siitä, onko viimeisin AWS-lähetys onnistunut.
// Käytetään historian jälkilähetyksessä indeksin siirtämiseen.
bool aws_client_last_send_ok(void)
{
    return s.last_send_ok;
}

// Kutsutaan pääsilmukasta. Sulkee valmistuneen yhteyden ja katkaisee
// pyynnön, jos sille annettu 30 sekunnin aika loppuu.
void aws_client_poll(void)
{
    if (!s.busy)
    {
        return;
    }

    if (s.finish)
    {
        drop_connection();
        return;
    }

    if (time_us_64() >= s.deadline)
    {
        if (s.dns_pending)
        {
            if (!s.dns_expired)
            {
                printf(
                    "AWS: DNS-aikakatkaisu; odotetaan haun siivousta.\n");
            }

            // Tilaa ei vapauteta vielä: myöhässä saapuva DNS-callback
            // käyttäisi sitä. dns_found() päättää pyynnön, kun se saapuu.
            s.dns_expired = true;
        }
        else
        {
            printf(
                "AWS: 30 s aikakatkaisu; tallennuksen tila tuntematon.\n");

            drop_connection();
        }
    }
}

// Lähettää yhden mittauksen AWS:ään HTTPS POST -pyynnöllä.
// timeStamp on tallennetun mittauksen aika, ei lähetysaika.
bool aws_submit_measurement(
    float temperature,
    float humidity,
    uint32_t measurement_timestamp,
    uint32_t current_timestamp)
{
    if (s.busy)
    {
        printf(
            "AWS: edellinen pyynto kesken, mittaus ohitetaan.\n");

        return false;
    }

    if (cyw43_tcpip_link_status(
            &cyw43_state,
            CYW43_ITF_STA) != CYW43_LINK_UP)
    {
        return false;
    }

    if (!measurement_timestamp ||
        !current_timestamp ||
        !isfinite(temperature) ||
        !isfinite(humidity))
    {
        return false;
    }

    if (!strcmp(
            AWS_AUTHORIZATION,
            "LISAA_AUTHORIZATION_AVAIN"))
    {
        printf(
            "AWS: tayta aws_settings.h:n authorization-avain.\n");

        return false;
    }

    if (!safe_setting(AWS_SENSOR_ID) ||
        !safe_setting(AWS_SENSOR_NAME) ||
        strchr(AWS_AUTHORIZATION, '\r') ||
        strchr(AWS_AUTHORIZATION, '\n'))
    {
        printf("AWS: asetuksissa ei-tuettu merkki.\n");
        return false;
    }

    // Päivitetään TLS:n käyttämä kello nykyhetkeen (ks. tls_time).
    // Mittauksen oma aikaleima lähetetään erikseen JSON-sanomassa.
    s.clock_base = current_timestamp;
    s.clock_us = time_us_64();

    if (!s.config)
    {
        // Aikafunktio riittää asettaa kerran, koska tls_time() lukee
        // yllä päivitetyn kellon joka kerta.
        mbedtls_platform_set_time(tls_time);

        s.config = altcp_tls_create_config_client(
            aws_root_ca,
            sizeof(aws_root_ca));

        if (!s.config)
        {
            printf("AWS: TLS-asetusten luonti epaonnistui.\n");
            return false;
        }
    }

    char body[384];

    // JSON sisältää sensorin tunnisteen, nimen, lämpötilan,
    // kosteuden ja tallennetun mittausajan.
    int n = snprintf(
        body,
        sizeof(body),
        "{\"sensorId\":\"%s\","
        "\"sensorName\":\"%s\","
        "\"temperature\":%.1f,"
        "\"humidity\":%.1f,"
        "\"timeStamp\":%" PRIu32 "}",
        AWS_SENSOR_ID,
        AWS_SENSOR_NAME,
        (double)temperature,
        (double)humidity,
        measurement_timestamp);

    if (n < 0 || (size_t)n >= sizeof(body))
    {
        return false;
    }

    int length = snprintf(
        s.request,
        sizeof(s.request),
        "POST " AWS_PATH " HTTP/1.1\r\n"
        "Host: " AWS_HOST "\r\n"
        "Authorization: %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        AWS_AUTHORIZATION,
        n,
        body);

    if (length < 0 || (size_t)length >= sizeof(s.request))
    {
        return false;
    }

    s.request_len = (size_t)length;
    s.received = 0;
    s.response[0] = 0;
    s.http_status = 0;
    s.headers_complete = false;
    s.finish = false;
    s.dns_expired = false;
    s.last_send_ok = false;
    s.busy = true;
    s.deadline = time_us_64() + 30000000ULL;

    printf(
        "AWS: lahetetaan mittaus, aikaleima %" PRIu32 ".\n",
        measurement_timestamp);

    ip_addr_t ip;
    s.dns_pending = true;

    err_t err = dns_gethostbyname(
        AWS_HOST,
        &ip,
        dns_found,
        NULL);

    if (err == ERR_OK)
    {
        s.dns_pending = false;
        connect_ip(&ip);
    }
    else if (err != ERR_INPROGRESS)
    {
        s.dns_pending = false;
        s.finish = true;

        printf("AWS: DNS-aloitusvirhe %d.\n", err);
    }

    return true;
}