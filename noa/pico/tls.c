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

typedef struct TLS_CLIENT_T
{
    struct altcp_pcb *pcb;
    const char *measurement;
} TLS_CLIENT_T;

struct altcp_tls_config *tls_config;

void tls_add_cert(const char *cert, size_t cert_len)
{
    tls_config = altcp_tls_create_config_client(cert, cert_len);
    if (!tls_config)
    {
        printf("tls-config-virhe\n");
    }
}
static err_t tls_client_close(void *arg)
{
    TLS_CLIENT_T *state = (TLS_CLIENT_T *)arg;
    err_t err = ERR_OK;

    if (state->pcb != NULL)
    {
        altcp_arg(state->pcb, NULL);
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
    return err;
}

static TLS_CLIENT_T *tls_init()
{
    // VARATAAN MUISTIA STATE-MUUTTUJALLE
    TLS_CLIENT_T *state = calloc(1, sizeof(TLS_CLIENT_T));
    if (!state)
    {
        printf("Muistinvaraus virhe.\n");
        return NULL;
    }
    return state;
}
static err_t tls_client_connected(void *arg, struct altcp_pcb *pcb, err_t err)
{
    TLS_CLIENT_T *state = (TLS_CLIENT_T *)arg;
    if (err != ERR_OK)
    {
        printf("Palvelimeen yhdistäminen epäonnistui\n");
        return tls_client_close(state);
    }

    printf("Yhteys palvelimeen kunnossa\n");
    // YHTEYS KUNNOSSA TLS AVULLA
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
                           AWS_HOST, AWS_AUTHORIZATION, strlen(state->measurement), state->measurement);
    err_t post_err = altcp_write(state->pcb, http_post_request, req_len, TCP_WRITE_FLAG_COPY);
    if (post_err != ERR_OK)
    {
        printf("Virhe datan lähetyksessä: %d\n", post_err);
        return tls_client_close(state);
    }
    altcp_output(state->pcb);
    return ERR_OK;
}

static err_t tls_client_poll(void *arg, struct altcp_pcb *pcb)
{
    TLS_CLIENT_T *state = (TLS_CLIENT_T *)arg;
    printf("timed out\n");
    return tls_client_close(arg);
}
static void tls_client_err(void *arg, err_t err)
{
    TLS_CLIENT_T *state = (TLS_CLIENT_T *)arg;
    printf("Virhe: tls_client_err callbackissa %d\n", err);
    tls_client_close(state);
}
static err_t tls_client_recv(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err)
{
    TLS_CLIENT_T *state = (TLS_CLIENT_T *)arg;
    if (!p)
    {
        printf("Yhteys suljettu\n");
        return tls_client_close(state);
    }

    if (p->tot_len > 0)
    {
        printf("Dataa vastaanotettu %d tavua.\n", p->tot_len);
        altcp_recved(state->pcb, p->tot_len);
    }
    pbuf_free(p);

    return ERR_OK;
}
static void tls_client_connect_to_server_ip(const ip_addr_t *ipaddr, TLS_CLIENT_T *state)
{
    err_t err;
    u16_t port = 443; // HTTPS

    // YHDISTETÄÄN TLS AVULLA
    printf("Yhdistetään %s\n",
           ipaddr_ntoa(ipaddr));
    err = altcp_connect(state->pcb, ipaddr, port, tls_client_connected);
    if (err != ERR_OK)
    {
        printf("Virhe TLS-yhdistämisessä: %d\n", err);
        tls_client_close(state);
    }
}
static void tls_client_dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg)
{
    if (ipaddr)
    {
        tls_client_connect_to_server_ip(ipaddr, (TLS_CLIENT_T *)arg);
    }
    else
    {
        printf("IP-osoitetta ei löytynyt hostille %s\n", hostname);
        tls_client_close(arg);
    }
}
static bool tls_client_open(const char *hostname, void *arg)
{
    // MUUTETAAN ANNETTU (SERVER) HOSTNAME IP-OSOITTEEKSI
    err_t err;
    ip_addr_t server_ip;
    TLS_CLIENT_T *state = (TLS_CLIENT_T *)arg;

    // ASETETAAN PCB
    state->pcb = altcp_tls_new(tls_config, IPADDR_TYPE_ANY);
    if (!state->pcb)
    {
        printf("Virhe PCB\n");
        return false;
    }

    // MÄÄRITELLÄÄN CALLBACKIT
    altcp_arg(state->pcb, state); // STATE KULKEE MUKANA
    altcp_recv(state->pcb, tls_client_recv);
    altcp_err(state->pcb, tls_client_err);
    altcp_poll(state->pcb, tls_client_poll, 40);
    // MÄÄRITELLÄÄN SNI
    mbedtls_ssl_set_hostname(altcp_tls_context(state->pcb), hostname);

    // ALOITETAAN ITSE DNS-KYSELY
    err = dns_gethostbyname(hostname, &server_ip, tls_client_dns_found, state);
    if (err == ERR_OK)
    {
        // DNS-CACHE OK
        tls_client_connect_to_server_ip(&server_ip, state);
    }
    else if (err != ERR_INPROGRESS)
    {
        printf("DNS virhe %d\n", err);
        tls_client_close(state->pcb);
    }
    return err == ERR_OK || err == ERR_INPROGRESS;
}
bool http_send(const char *measurement, const char *server)
{
    // LÄHETETÄÄN POST-PYYNTÖ

    // LUODAAN TÄLLE YHTEYDELLE OMA TLS_CLIENT_T TYYPPIÄ OLEVA MUUTTUJA
    TLS_CLIENT_T *state = tls_init();
    if (!state)
    {
        printf("Muistin varaaminen epäonnistui\n");
        return false;
    }

    // TALLENNETAAN POINTTERI MITTAUSTULOKSIIN
    state->measurement = measurement;

    if (!tls_config)
    {
        printf("TLS-config puuttuu\n");
        return false;
    }

    // KÄYNNISTETÄÄN DNS-HAKU ANNETUSTA SERVERISTÄ
    if (!tls_client_open(server, state))
    {
        // YHTEYS EI VOITU MUODOSTAA > LÄHETYS EPÄONNISTUNUT
        printf("Yhteyttä ei voitu muodostaa\n");
        return false;
    }
    return true;
}