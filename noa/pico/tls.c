/*
 * Copyright (c) 2023 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "stdlib.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/dns.h"

#include "aws_config.h"

// TYPEDEFFIT
typedef struct TLS_CLIENT_T
{
    struct altcp_pcb *pcb;
    struct altcp_tls_config *tls_config;
    char measurement_json[512];
} TLS_CLIENT_T;

// FUNKTIOT MÄÄRITTELY
TLS_CLIENT_T *http_init(const char *measurement_json);
void dns_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg);
err_t tls_connected(void *arg, struct altcp_pcb *conn, err_t err);
err_t tls_recv(void *arg, struct altcp_pcb *conn, struct pbuf *p, err_t err);
static err_t tls_client_close(void *arg);

// FUNKTIOT
static err_t tls_client_close(void *arg)
{
    TLS_CLIENT_T *state = (TLS_CLIENT_T *)arg;
    err_t err = ERR_OK;

    if (state->pcb != NULL)
    {
        altcp_arg(state->pcb, NULL);
        altcp_poll(state->pcb, NULL, 0);
        altcp_recv(state->pcb, NULL);
        altcp_err(state->pcb, NULL);
        err = altcp_close(state->pcb);
        if (err != ERR_OK)
        {
            printf("close failed %d, calling abort\n", err);
            altcp_abort(state->pcb);
            err = ERR_ABRT;
        }

        state->pcb = NULL;
    }
    if (state->tls_config)
    {
        altcp_tls_free_config(state->tls_config);
    }
    free(state);
    return err;
}
TLS_CLIENT_T *http_init(const char *measurement_json)
{
    // LUODAAN UUSI STRUCT
    printf("http_init() käynnissä.\n\n");
    TLS_CLIENT_T *connection_state = calloc(1, sizeof(TLS_CLIENT_T));
    if (!connection_state)
    {
        printf("Virhe muistinvarauksessa.\n");
        return NULL;
    }
    // ALUSTETAAN TLS_CONFIG
    connection_state->tls_config = altcp_tls_create_config_client(aws_cert, sizeof(aws_cert));
    if (!connection_state->tls_config)
    {
        printf("Virhe tls_config.\n");
        free(connection_state);
        return NULL;
    }
    // ALUSTETAAN PCB
    connection_state->pcb = altcp_tls_new(connection_state->tls_config, IPADDR_TYPE_ANY);
    if (!connection_state->pcb)
    {
        printf("Virhe PCB kanssa.\n");
        altcp_tls_free_config(connection_state->tls_config);
        free(connection_state);
        return NULL;
    }
    // TALLENNETAAN MEASUREMENT STATEEN
    snprintf(connection_state->measurement_json, sizeof(connection_state->measurement_json), "%s", measurement_json);

    // KULJETAAN CONNECTION_STATE STRUCT (POINTTERI) AINA MUKANA CALLBACKEISSA
    altcp_arg(connection_state->pcb, connection_state);
    // MÄÄRITELLÄÄN CALLBACKIT
    altcp_recv(connection_state->pcb, tls_recv);
    mbedtls_ssl_set_hostname(altcp_tls_context(connection_state->pcb), AWS_HOST);

    ip_addr_t server_address;
    dns_gethostbyname(AWS_HOST, &server_address, dns_callback, connection_state);
    return connection_state;
}

void dns_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    // MUKANA TULLUT TLS_CLIENT_T ELÄÄ *CALLBACK_ARG
    TLS_CLIENT_T *connection_state = (TLS_CLIENT_T *)callback_arg;

    if (!ipaddr)
    {
        printf("DNS-kysely ei löytänyt IP-osoitetta.\n");
        return;
    }
    // IP OSOITE LÖYTYNYT > YHDISTETÄÄN SIIHEN TLS AVULLA
    err_t err = altcp_connect(connection_state->pcb, ipaddr, 443, tls_connected);
    if (err != ERR_OK)
    {
        printf("Virhe yhdistäesssä palvelimeen %s.\n", ipaddr_ntoa(ipaddr));
        tls_client_close(connection_state);
    }
}

err_t tls_connected(void *arg, struct altcp_pcb *conn, err_t err)
{
    TLS_CLIENT_T *connection_state = (TLS_CLIENT_T *)arg;
    if (err != ERR_OK)
    {
        printf("Yhdistys epäonnistui. Koodi:.%d\n", err);
        return tls_client_close(connection_state);
    }
    printf("Yhdisttetty TLS avulla!\n");

    // 2. Rakennetaan HTTP POST -pyyntö dynaamisesti, jotta Content-Length täsmää
    char http_post_request[768];
    int req_len = snprintf(http_post_request, sizeof(http_post_request),
                           "POST /measurement/save HTTP/1.1\r\n"
                           "Host: %s\r\n"
                           "Authorization: %s\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "%s",
                           AWS_HOST, AWS_AUTHORIZATION, strlen(connection_state->measurement_json), connection_state->measurement_json);
    err_t write_err = altcp_write(conn, http_post_request, strlen(http_post_request), TCP_WRITE_FLAG_COPY);
    if (write_err != ERR_OK)
    {
        printf("Virhe datan lähetyksessä: %d\n", write_err);
        return tls_client_close(connection_state);
    }

    altcp_output(conn);
    return ERR_OK;
}
err_t tls_recv(void *arg, struct altcp_pcb *conn, struct pbuf *p, err_t err)
{
    TLS_CLIENT_T *connection_state = (TLS_CLIENT_T *)arg;
    if (!p)
    {
        printf("Palvelin sulki yhteyden.\n");
        return tls_client_close(connection_state);
    }
    if (p->tot_len > 0)
    {
        printf("Dataa vastaanotettu %d tavua.\n", p->tot_len);
        for (struct pbuf *q = p; q != NULL; q = q->next)
        {
            printf("%.*s", q->len, (char *)q->payload);
        }
        printf("\n");
        altcp_recved(connection_state->pcb, p->tot_len);
    }
    pbuf_free(p);
    return ERR_OK;
}