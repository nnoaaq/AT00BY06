/*
    Muokattu Noan tekemän pohjalta, joka oli muokattu Raspin esimerkistä. Kommentit tekoälyn tekemiä.
 * Copyright (c) 2023 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/dns.h"

#include "aws_send.h"
#include "aws_certs.h"
#include "aws_ca.h"
#include "tls.h"

#ifndef AWS_REQUEST_TIMEOUT_MS
#define AWS_REQUEST_TIMEOUT_MS 10000
#endif

// Yhden HTTP/TLS-pyynnön tilatiedot. Välitetään lwIP-takaisinkutsuihin (callback) arg-osoittimella.
typedef struct TLS_CLIENT_T_
{
    struct altcp_pcb *pcb;              // lwIP altcp-yhteysrakenne (TCP + TLS)
    struct altcp_tls_config *tls_config; // mbedTLS-asetukset ja CA-juurivarmenne
    char measurement_json[1024];         // Lähetettävä JSON-runko
    char recv_buf[256];                  // Puskuri HTTP-vastauksen alkupäälle
    size_t recv_len;                     // Vastaanotetun datan pituus recv_buf-puskurissa
    bool done;                           // True, kun pyyntö on päättynyt (onnistui, epäonnistui tai aikakatkaistiin)
    bool success;                        // True vain, jos palvelin vastasi HTTP 2xx -tilakoodilla
} TLS_CLIENT_T;


static err_t tls_client_close(void *arg);
static void dns_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg);
static err_t tls_connected(void *arg, struct altcp_pcb *conn, err_t err);
static err_t tls_recv(void *arg, struct altcp_pcb *conn, struct pbuf *p, err_t err);
static void tls_err(void *arg, err_t err);

// Merkitsee pyynnön päättyneeksi ja asettaa lopputuloksen kutsuvaa silmukkaa varten.
static void mark_finished(TLS_CLIENT_T *state, bool ok)
{
    if (!state)
    {
        return;
    }
    state->success = ok;
    state->done = true;
}

// Sulkee TLS/TCP-yhteyden siististi ja vapauttaa TLS-konfiguraation.
// Kutsutaan silloin, kun yhteys on edelleen olemassa lwIP:n puolella.
static err_t tls_client_close(void *arg)
{
    TLS_CLIENT_T *state = (TLS_CLIENT_T *)arg;
    if (!state)
    {
        return ERR_OK;
    }

    err_t err = ERR_OK;

    if (state->pcb != NULL)
    {
        // Irrotetaan callbackit ennen sulkemista, jotta uusia kutsuja ei enää tule
        altcp_arg(state->pcb, NULL);
        altcp_poll(state->pcb, NULL, 0);
        altcp_recv(state->pcb, NULL);
        altcp_err(state->pcb, NULL);

        err = altcp_close(state->pcb);
        if (err != ERR_OK)
        {
            // Jos siisti sulkeminen ei onnistu, pakotetaan yhteys alas (abort)
            printf("tls_client_close: altcp_close epäonnistui (%d), kutsutaan altcp_abort\n", err);
            altcp_abort(state->pcb);
            err = ERR_ABRT;
        }
        state->pcb = NULL;
    }

    if (state->tls_config != NULL)
    {
        altcp_tls_free_config(state->tls_config);
        state->tls_config = NULL;
    }

    state->done = true;
    return err;
}

// lwIP kutsuu tätä vakavassa TCP/TLS-virheessä (esim. yhteys katkeaa tai nollautuu).
// Huom: lwIP vapauttaa pcb:n itse tämän callbackin jälkeen, joten pcb:tä ei saa kutsua altcp_close:lla.
static void tls_err(void *arg, err_t err)
{
    TLS_CLIENT_T *state = (TLS_CLIENT_T *)arg;
    printf("TLS/TCP-virhe: %d\n", err);
    if (state)
    {
        state->pcb = NULL;
        if (state->tls_config)
        {
            altcp_tls_free_config(state->tls_config);
            state->tls_config = NULL;
        }
        mark_finished(state, false);
    }
}

// Käsittelee palvelimelta saapuvan TLS-salatun HTTP-vastauksen paloina (pbuf-ketju).
static err_t tls_recv(void *arg, struct altcp_pcb *conn, struct pbuf *p, err_t err)
{
    (void)conn;
    TLS_CLIENT_T *state = (TLS_CLIENT_T *)arg;
    if (!state)
    {
        if (p)
        {
            pbuf_free(p);
        }
        return ERR_OK;
    }

    if (err != ERR_OK)
    {
        printf("tls_recv virhe: %d\n", err);
        if (p)
        {
            pbuf_free(p);
        }
        tls_client_close(state);
        mark_finished(state, false);
        return err;
    }

    // p == NULL tarkoittaa, että etäpalvelin on sulkenut yhteyden (FIN).
    if (!p)
    {
        bool http_ok = (strstr(state->recv_buf, "HTTP/1.1 200") != NULL) ||
                       (strstr(state->recv_buf, "HTTP/1.1 201") != NULL) ||
                       (strstr(state->recv_buf, "HTTP/1.1 202") != NULL) ||
                       (strstr(state->recv_buf, "HTTP/1.0 200") != NULL);
        tls_client_close(state);
        mark_finished(state, http_ok);
        return ERR_OK;
    }

    if (p->tot_len > 0)
    {
        // Kopioidaan vastauksen alkua recv_buf-puskuriin tilakoodin tarkistusta varten
        size_t space = (sizeof(state->recv_buf) - 1) - state->recv_len;
        size_t to_copy = (p->tot_len < space) ? p->tot_len : space;
        if (to_copy > 0)
        {
            pbuf_copy_partial(p, state->recv_buf + state->recv_len, (u16_t)to_copy, 0);
            state->recv_len += to_copy;
            state->recv_buf[state->recv_len] = '\0';
        }

        // Tulostetaan saapunut data sarjaporttiin
        for (struct pbuf *q = p; q != NULL; q = q->next)
        {
            printf("%.*s", q->len, (char *)q->payload);
        }
        printf("\n");

        // Ilmoitetaan lwIP:lle, että vastaanotettu data on käsitelty
        altcp_recved(state->pcb, p->tot_len);
    }

    pbuf_free(p);
    return ERR_OK;
}

// Kutsutaan, kun TCP-yhteys ja TLS-kättely (handshake) palvelimeen ovat onnistuneet.
static err_t tls_connected(void *arg, struct altcp_pcb *conn, err_t err)
{
    TLS_CLIENT_T *state = (TLS_CLIENT_T *)arg;
    if (!state)
    {
        return ERR_ARG;
    }

    if (err != ERR_OK)
    {
        printf("TLS-yhteys epäonnistui, koodi: %d\n", err);
        tls_client_close(state);
        mark_finished(state, false);
        return err;
    }

    printf("Yhdistetty TLS:llä!\n");

    // Rakennetaan HTTP/1.1 POST -pyyntö; Content-Lengthin täytyy vastata tarkalleen JSON-rungon tavumäärää.
    char http_post_request[1536];
    size_t body_len = strlen(state->measurement_json);
    int req_len = snprintf(
        http_post_request,
        sizeof(http_post_request),
        "POST %s HTTP/1.1\r\n"
        "Host: %s \r\n"
        "Authorization: %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        AWS_PATH,
        AWS_HOST,
        AWS_AUTHORIZATION,
        body_len,
        state->measurement_json);

    if (req_len < 0 || (size_t)req_len >= sizeof(http_post_request))
    {
        printf("HTTP-pyyntö ei mahdu puskuriin.\n");
        tls_client_close(state);
        mark_finished(state, false);
        return ERR_MEM;
    }

    // Kirjoitetaan pyyntö lwIP:n lähetyspuskuriin (TCP_WRITE_FLAG_COPY kopioi datan omaan puskuriin)
    err_t write_err = altcp_write(conn, http_post_request, (u16_t)req_len, TCP_WRITE_FLAG_COPY);
    if (write_err != ERR_OK)
    {
        printf("Virhe datan kirjoituksessa: %d\n", write_err);
        tls_client_close(state);
        mark_finished(state, false);
        return write_err;
    }

    // Käynnistetään pakettien lähetys verkkoon heti
    err_t out_err = altcp_output(conn);
    if (out_err != ERR_OK)
    {
        printf("Virhe altcp_output-kutsussa: %d\n", out_err);
        tls_client_close(state);
        mark_finished(state, false);
        return out_err;
    }

    return ERR_OK;
}

// DNS-kyselyn takaisinkutsu: jatkaa TLS-yhteyden avaamista, kun IP-osoite on selvitetty.
static void dns_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    (void)name;
    TLS_CLIENT_T *state = (TLS_CLIENT_T *)callback_arg;
    if (!state)
    {
        return;
    }

    if (!ipaddr)
    {
        printf("DNS-kysely ei löytänyt IP-osoitetta.\n");
        tls_client_close(state);
        mark_finished(state, false);
        return;
    }

    // Käynnistää TCP-yhteyden ja TLS-kättelyn määritettyyn porttiin (yleensä 443)
    err_t err = altcp_connect(state->pcb, ipaddr, AWS_PORT, tls_connected);
    if (err != ERR_OK)
    {
        printf("Virhe yhdistettäessä palvelimeen %s (koodi %d).\n", ipaddr_ntoa(ipaddr), err);
        tls_client_close(state);
        mark_finished(state, false);
    }
}

// Lähettää JSON-datan HTTPS POST -pyynnöllä ja odottaa, kunnes vastaus on saatu tai aikakatkaistu.
bool send_http_post(const char *host, uint16_t port, const char *path, const char *auth, const char *json_body)
{

    if (!host || !json_body)
    {
        return false;
    }

    TLS_CLIENT_T *state = calloc(1, sizeof(TLS_CLIENT_T));
    if (!state)
    {
        printf("Virhe muistinvarauksessa (TLS_CLIENT_T).\n");
        return false;
    }

    // Luodaan TLS-asiakaskonfiguraatio palvelimen varmenteen (aws_ca.h) todentamista varten
    state->tls_config = altcp_tls_create_config_client(aws_cert, sizeof(aws_cert));
    if (!state->tls_config)
    {
        printf("Virhe tls_config-alustuksessa.\n");
        free(state);
        return false;
    }

    // Luodaan TLS-kääreellä varustettu altcp-yhteysolio
    state->pcb = altcp_tls_new(state->tls_config, IPADDR_TYPE_ANY);
    if (!state->pcb)
    {
        printf("Virhe PCB-alustuksessa.\n");
        altcp_tls_free_config(state->tls_config);
        free(state);
        return false;
    }

    snprintf(state->measurement_json, sizeof(state->measurement_json), "%s", json_body);

    // Liitetään tilatieto ja takaisinkutsut yhteyteen sekä asetetaan SNI-isäntänimi TLS-kättelyyn
    altcp_arg(state->pcb, state);
    altcp_recv(state->pcb, tls_recv);
    altcp_err(state->pcb, tls_err);
    mbedtls_ssl_set_hostname(altcp_tls_context(state->pcb), host);

    // lwIP-kutsujen ympärillä käytetään lukitusta (poll-tilassa no-op, mutta turvallinen muiden tilojen varalta)
    cyw43_arch_lwip_begin();
    ip_addr_t resolved_ip;
    err_t dns_err = dns_gethostbyname(host, &resolved_ip, dns_callback, state);
    if (dns_err == ERR_OK)
    {
        // IP löytyi suoraan DNS-välimuistista, jatketaan heti yhteydenmuodostukseen
        dns_callback(host, &resolved_ip, state);
    }
    else if (dns_err != ERR_INPROGRESS)
    {
        printf("DNS-kysely käynnistyi virheellä: %d\n", dns_err);
        tls_client_close(state);
        mark_finished(state, false);
    }
    cyw43_arch_lwip_end();

    // Pollaa Wi-Fi- ja lwIP-työtä, kunnes callbackit merkitsevät pyynnön valmiiksi tai aikaraja täyttyy
    absolute_time_t deadline = make_timeout_time_ms(AWS_REQUEST_TIMEOUT_MS);
    while (!state->done && !time_reached(deadline))
    {
        cyw43_arch_poll();
        sleep_ms(10);
    }

    if (!state->done)
    {
        printf("AWS HTTP POST aikakatkaistiin.\n");
        cyw43_arch_lwip_begin();
        tls_client_close(state);
        cyw43_arch_lwip_end();
        mark_finished(state, false);
    }

    bool ok = state->success;
    free(state);
    return ok;
}