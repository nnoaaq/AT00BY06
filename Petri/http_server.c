#include "http_server.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/rand.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "measurement.h"

#define HTTP_PORT 80
#define HTTP_REQUEST_CAPACITY 2048u // pyyntö (otsakkeet + runko) enintään tämän kokoinen
#define HTTP_BODY_LIMIT 128u        // pyynnön rungolle sallittu enimmäispituus
#define HTTP_IDLE_POLL_LIMIT 15     // katkaistaan yhteys, kun näin monta pollia on kulunut
#define HTTP_POLL_INTERVAL 2        // lwIP:n poll-väli (2 x 500 ms = 1 s)
#define STATUS_PAGE_CAPACITY 3072u

// Yhden asiakasyhteyden tila. Varataan yhteyden avautuessa ja vapautetaan
// sen sulkeuduttua tai katketessa.
typedef struct
{
    uint32_t unacknowledged; // lähetetty mutta kuittaamatta olevat tavut
    unsigned idle_polls;     // poll-kutsuja viimeisimmän liikenteen jälkeen
    bool response_sent;
    size_t used;             // pyyntöpuskurin käytetty koko
    char request[HTTP_REQUEST_CAPACITY];
} HttpClient;

// Pyynnöstä poimitut tiedot.
typedef struct
{
    char method[8];
    char path[64];
    size_t body_length;
    bool have_length;
    bool form_type; // Content-Type on application/x-www-form-urlencoded
} ParsedRequest;

static struct tcp_pcb *http_listener = NULL;
static HttpStatusFn status_callback = NULL;
static char settings_token[33];
static unsigned pending_interval_minutes = 0;

static err_t http_sent(void *arg, struct tcp_pcb *pcb, u16_t length);
static err_t http_poll(void *arg, struct tcp_pcb *pcb);
static err_t http_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);

// ---------------- Yhteyden hallinta ----------------

// lwIP kutsuu tätä, kun yhteys on katkennut virheeseen. PCB on silloin jo
// vapautettu, joten vain asiakastieto vapautetaan.
static void http_error(void *arg, err_t err)
{
    (void)err;
    free(arg);
}

// Katkaisee yhteyden välittömästi (RST) ja vapauttaa asiakastiedon.
static err_t http_abort_client(struct tcp_pcb *pcb, HttpClient *client)
{
    tcp_arg(pcb, NULL);
    tcp_err(pcb, NULL);
    free(client);
    tcp_abort(pcb);
    return ERR_ABRT;
}

// Sulkee yhteyden siististi. Jos sulkeminen epäonnistuu (esim. muisti
// loppu), callbackit palautetaan ja sulkemista yritetään uudelleen
// seuraavalla pollilla.
static err_t http_try_close(struct tcp_pcb *pcb, HttpClient *client)
{
    // Callbackit poistetaan ENNEN sulkemista, jottei lwIP kutsu niitä
    // vapautetulle asiakastiedolle.
    tcp_arg(pcb, NULL);
    tcp_err(pcb, NULL);
    tcp_recv(pcb, NULL);
    tcp_sent(pcb, NULL);
    tcp_poll(pcb, NULL, 0);

    err_t result = tcp_close(pcb);

    if (result == ERR_OK)
    {
        free(client);
        return ERR_OK;
    }

    tcp_arg(pcb, client);
    tcp_err(pcb, http_error);
    tcp_recv(pcb, http_recv);
    tcp_sent(pcb, http_sent);
    tcp_poll(pcb, http_poll, HTTP_POLL_INTERVAL);

    return ERR_OK;
}

// Vastapuoli on kuitannut lähetettyjä tavuja. Kun kaikki on kuitattu,
// yhteys voidaan sulkea.
static err_t http_sent(void *arg, struct tcp_pcb *pcb, u16_t length)
{
    HttpClient *client = arg;

    if (!client)
    {
        return ERR_OK;
    }

    client->idle_polls = 0;

    client->unacknowledged =
        length >= client->unacknowledged ? 0 : client->unacknowledged - length;

    if (client->response_sent && !client->unacknowledged)
    {
        return http_try_close(pcb, client);
    }

    return ERR_OK;
}

// Säännöllinen poll: katkaisee jumiin jääneet yhteydet ja yrittää uudelleen
// sulkemista, jos se ei onnistunut aiemmin.
static err_t http_poll(void *arg, struct tcp_pcb *pcb)
{
    HttpClient *client = arg;

    if (!client)
    {
        return ERR_OK;
    }

    if (++client->idle_polls >= HTTP_IDLE_POLL_LIMIT)
    {
        return http_abort_client(pcb, client);
    }

    if (client->response_sent && !client->unacknowledged)
    {
        return http_try_close(pcb, client);
    }

    return ERR_OK;
}

// ---------------- Vastaukset ----------------

// Lähettää HTTP-vastauksen. status on esim. "200 OK", extra on valinnaisia
// lisäotsakkeita (kukin päättyen \r\n) ja body on HTML-teksti.
// Otsakkeet ja runko kirjoitetaan erikseen, jotta koko vastausta ei tarvitse
// koota pinossa olevaan puskuriin.
static err_t http_reply(
    struct tcp_pcb *pcb,
    HttpClient *client,
    const char *status,
    const char *extra,
    const char *body)
{
    char header[320];
    size_t body_length = strlen(body);

    int header_length = snprintf(
        header,
        sizeof(header),
        "HTTP/1.1 %s\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "X-Frame-Options: DENY\r\n"
        "%s\r\n",
        status,
        (unsigned)body_length,
        extra);

    if (header_length < 0 || (size_t)header_length >= sizeof(header))
    {
        return http_abort_client(pcb, client);
    }

    size_t total = (size_t)header_length + body_length;

    if (total > tcp_sndbuf(pcb))
    {
        return http_abort_client(pcb, client);
    }

    if (tcp_write(
            pcb,
            header,
            (u16_t)header_length,
            TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE) != ERR_OK)
    {
        return http_abort_client(pcb, client);
    }

    if (body_length > 0 &&
        tcp_write(pcb, body, (u16_t)body_length, TCP_WRITE_FLAG_COPY) != ERR_OK)
    {
        return http_abort_client(pcb, client);
    }

    client->response_sent = true;
    client->unacknowledged = (uint32_t)total;
    (void)tcp_output(pcb);

    return ERR_OK;
}

// Rakentaa ja lähettää tilasivun.
static err_t http_status_page(struct tcp_pcb *pcb, HttpClient *client)
{
    // Staattinen puskuri säästää pinoa. Palvelin toimii yhdessä säikeessä
    // (poll-arkkitehtuuri), joten yhteiskäyttö ei ole ongelma.
    static char body[STATUS_PAGE_CAPACITY];

    HttpStatus status;
    memset(&status, 0, sizeof(status));
    status_callback(&status);

    int n = snprintf(body, sizeof(body),
                     "<!doctype html><html lang='fi'><head><meta charset='utf-8'>"
                     "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                     "<title>Pico-sääasema</title><style>"
                     "body{font-family:'Courier New',monospace;background:#000;"
                     "color:#33ff66;padding:2rem;text-align:center}"
                     "main{display:inline-block;background:#000;padding:2rem;"
                     "border:2px solid #1f6f38;border-radius:6px;"
                     "min-width:340px;max-width:560px;text-align:left;"
                     "box-shadow:0 0 18px rgba(51,255,102,.25)}"
                     "h1{color:#33ff66;text-align:center;margin-top:0;"
                     "text-shadow:0 0 8px rgba(51,255,102,.55)}"
                     "h2{color:#33ff66;font-size:1.05rem;margin-top:1.4rem;"
                     "border-bottom:1px solid #1f6f38;padding-bottom:.3rem;"
                     "text-shadow:0 0 6px rgba(51,255,102,.45)}"
                     "p{font-size:1.05rem;margin:.4rem 0}"
                     ".value{color:#33ff66;font-weight:bold;"
                     "text-shadow:0 0 6px rgba(51,255,102,.45)}"
                     ".muted{color:#7a8a80;font-size:.95rem}"
                     "select,button{font-family:inherit;font-size:1rem;"
                     "background:#000;color:#33ff66;border:1px solid #1f6f38;"
                     "border-radius:4px;padding:.45rem .6rem}"
                     "button{cursor:pointer}"
                     "button:hover{background:#0a2a15}"
                     "</style></head><body><main><h1>Pico 2 W -sääasema</h1>"
                     "<h2>Viimeisen minuutin keskiarvo</h2>"
                     "<p>Tila: <span class='value'>%s</span></p>"
                     "<p>Lämpötila: <span class='value'>%.1f &deg;C</span></p>"
                     "<p>Ilmankosteus: <span class='value'>%.1f %%</span></p>"
                     "<p>Picon sisäinen lämpötila: <span class='value'>%.1f &deg;C</span></p>"
                     "<p>Akkuarvio: <span class='value'>%u %%</span></p>"
                     "<p class='muted'>Mittausaika: %s</p>"
                     "<p class='muted'>Historiassa: %" PRIu32 "/%u</p>"
                     "<h2>Tallennus ja AWS-lähetys</h2>"
                     "<p>Nykyinen jakso: %u minuuttia</p>"
                     "<p>Jakson päättymiseen noin %u min %u s</p>"
                     "<form method='post' action='/settings'>"
                     "<label for='interval'>Tallennus- ja lähetysväli:</label> "
                     // Sivun automaattinen päivitys pysäytetään, kun valikko
                     // aktivoidaan, ettei valinta katoa kesken kaiken.
                     "<select id='interval' name='interval' onfocus='clearTimeout(refreshTimer)'>"
                     "<option value='1'%s>Sauna-mode: 1 min tallennus, ei Wi-Fi/AWS</option>"
                     "<option value='15'%s>15 minuuttia</option>"
                     "<option value='60'%s>60 minuuttia</option></select> "
                     "<input type='hidden' name='token' value='%s'>"
                     "<button type='submit'>Ota käyttöön</button></form>"
                     "<small>Sauna-tila tallentaa minuutin välein, mutta katkaisee Wi-Fi-yhteyden "
                     "eikä lähetä AWS:ään. Uudelleenkäynnistyksen jälkeen oletus on 15 minuuttia, "
                     "Wi-Fi palaa ja tallennettu historia lähetetään uudelleen.</small>"
                     "<script>var refreshTimer=setTimeout(function(){location.reload()},10000);</script>"
                     "</main></body></html>",
                     status.measurement_valid ? "mittaus saatavilla" : "ei kelvollista minuuttimittausta",
                     (double)status.temperature,
                     (double)status.humidity,
                     (double)status.internal_temperature,
                     (unsigned)status.battery_percent,
                     status.measurement_time,
                     status.stored_count,
                     (unsigned)MAX_MEASUREMENTS,
                     status.interval_minutes,
                     status.seconds_to_period_end / 60,
                     status.seconds_to_period_end % 60,
                     status.interval_minutes == 1 ? " selected" : "",
                     status.interval_minutes == 15 ? " selected" : "",
                     status.interval_minutes == 60 ? " selected" : "",
                     settings_token);

    if (n < 0 || (size_t)n >= sizeof(body))
    {
        return http_abort_client(pcb, client);
    }

    return http_reply(pcb, client, "200 OK", "", body);
}

// ---------------- Pyynnön jäsennys ----------------

// Vertaa otsakkeen nimeä (start, size tavua) annettuun pieniin kirjaimiin
// kirjoitettuun nimeen kirjainkoosta riippumatta. HTTP-otsakkeiden nimet
// ovat kirjainkoosta riippumattomia.
static bool header_name(const char *start, size_t size, const char *name)
{
    if (strlen(name) != size)
    {
        return false;
    }

    for (size_t i = 0; i < size; ++i)
    {
        unsigned char c = (unsigned char)start[i];

        if (c >= 'A' && c <= 'Z')
        {
            c += 'a' - 'A';
        }

        if (c != (unsigned char)name[i])
        {
            return false;
        }
    }

    return true;
}

// Jäsentää lomakkeen rungon muotoa "interval=15&token=...".
// Hyväksyy vain tunnetut kentät kerran kukin, sallitut välit (1, 15, 60)
// ja oikean tunnisteen. Muuttaa runkoa (korvaa erottimet \0:lla).
static bool parse_setting(char *body, unsigned *minutes)
{
    bool have_interval = false;
    bool have_token = false;
    char *field = body;

    while (*field)
    {
        char *next = strchr(field, '&');

        if (next)
        {
            *next++ = '\0';
        }

        char *eq = strchr(field, '=');

        if (!eq)
        {
            return false;
        }

        *eq++ = '\0';

        if (!strcmp(field, "interval") && !have_interval)
        {
            if (!strcmp(eq, "1"))
            {
                *minutes = 1;
            }
            else if (!strcmp(eq, "15"))
            {
                *minutes = 15;
            }
            else if (!strcmp(eq, "60"))
            {
                *minutes = 60;
            }
            else
            {
                return false;
            }

            have_interval = true;
        }
        else if (!strcmp(field, "token") && !have_token)
        {
            if (strcmp(eq, settings_token))
            {
                return false;
            }

            have_token = true;
        }
        else
        {
            // Tuntematon tai kahteen kertaan annettu kenttä.
            return false;
        }

        if (!next)
        {
            break;
        }

        field = next;
    }

    return have_interval && have_token;
}

// Käy läpi otsakerivit (line..header_end) ja poimii Content-Length-,
// Content-Type- ja Transfer-Encoding-otsakkeet.
// Palauttaa virheilmoituksen (vastataan 400), tai NULL jos kaikki on kunnossa.
static const char *parse_headers(
    char *line,
    const char *header_end,
    ParsedRequest *req)
{
    while (line < header_end)
    {
        char *end = strstr(line, "\r\n");
        char *colon = memchr(line, ':', (size_t)(end - line));

        if (!colon)
        {
            return "Virheellinen otsake.";
        }

        // Arvo alkaa kaksoispisteen jälkeisten välilyöntien jälkeen ja
        // päättyy ennen riviä lopettavia välilyöntejä.
        char *value = colon + 1;

        while (value < end && (*value == ' ' || *value == '\t'))
        {
            value++;
        }

        char *value_end = end;

        while (value_end > value && (value_end[-1] == ' ' || value_end[-1] == '\t'))
        {
            value_end--;
        }

        size_t name_length = (size_t)(colon - line);

        if (header_name(line, name_length, "content-length"))
        {
            // Vain yksi Content-Length sallitaan, ja sen pitää olla luku.
            if (req->have_length || value == value_end)
            {
                return "Virheellinen pituus.";
            }

            req->have_length = true;

            for (char *q = value; q < value_end; ++q)
            {
                // Pituus tarkistetaan jo jokaisella numerolla, jottei luku
                // ehdi ylivuotaa.
                if (*q < '0' || *q > '9' || req->body_length > HTTP_BODY_LIMIT)
                {
                    return "Virheellinen pituus.";
                }

                req->body_length = req->body_length * 10 + (unsigned)(*q - '0');
            }
        }
        else if (header_name(line, name_length, "transfer-encoding"))
        {
            return "Chunked-pyyntöjä ei tueta.";
        }
        else if (header_name(line, name_length, "content-type"))
        {
            req->form_type = header_name(
                value,
                (size_t)(value_end - value),
                "application/x-www-form-urlencoded");
        }

        line = end + 2;
    }

    return NULL;
}

// Käsittelee kokonaan vastaanotetun pyynnön. client->request sisältää
// otsakkeet (loppuen kohtaan header_end) ja mahdollisen rungon.
static err_t http_handle_request(
    struct tcp_pcb *pcb,
    HttpClient *client,
    char *header_end)
{
    // Pyyntörivi: "METODI POLKU HTTP/1.x". Neljäs kenttä (extra) kertoo,
    // onko rivillä ylimääräistä tekstiä, jolloin pyyntö hylätään.
    char *line_end = strstr(client->request, "\r\n");

    if (!line_end)
    {
        return http_reply(pcb, client, "400 Bad Request", "", "Virheellinen otsake.");
    }

    ParsedRequest req;
    memset(&req, 0, sizeof(req));

    char version[16];
    char extra;

    *line_end = 0;

    int fields = sscanf(
        client->request,
        "%7s %63s %15s %c",
        req.method,
        req.path,
        version,
        &extra);

    *line_end = '\r';

    if (fields != 3 ||
        (strcmp(version, "HTTP/1.1") && strcmp(version, "HTTP/1.0")))
    {
        return http_reply(pcb, client, "400 Bad Request", "", "Virheellinen pyyntörivi.");
    }

    const char *error = parse_headers(line_end + 2, header_end, &req);

    if (error)
    {
        return http_reply(pcb, client, "400 Bad Request", "", error);
    }

    // Rungon paikka ja koko puskurissa.
    size_t offset = (size_t)(header_end + 4 - client->request);

    if (req.body_length > HTTP_BODY_LIMIT ||
        offset + req.body_length >= sizeof(client->request))
    {
        return http_reply(pcb, client, "413 Content Too Large", "", "Sisältö liian suuri.");
    }

    // Runko ei ole vielä kokonaan perillä: odotetaan lisää dataa.
    if (client->used < offset + req.body_length)
    {
        return ERR_OK;
    }

    // Reititys.
    if (!strcmp(req.method, "GET") && !strcmp(req.path, "/"))
    {
        return http_status_page(pcb, client);
    }

    if (!strcmp(req.path, "/settings"))
    {
        if (strcmp(req.method, "POST"))
        {
            return http_reply(pcb, client, "405 Method Not Allowed", "Allow: POST\r\n", "Käytä sivun lomaketta.");
        }

        if (!req.have_length || !req.form_type)
        {
            return http_reply(pcb, client, "400 Bad Request", "", "Virheellinen lomake.");
        }

        client->request[offset + req.body_length] = 0;

        unsigned minutes = 0;

        if (!parse_setting(client->request + offset, &minutes))
        {
            return http_reply(pcb, client, "403 Forbidden", "", "Virheellinen valinta tai tunniste. Avaa etusivu uudelleen.");
        }

        // Pääsilmukka ottaa pyynnön käyttöön (http_server_take_interval_request).
        pending_interval_minutes = minutes;

        return http_reply(pcb, client, "303 See Other", "Location: /\r\n", "Asetus hyväksytty. Palaa etusivulle.");
    }

    if (strcmp(req.method, "GET") && strcmp(req.method, "POST"))
    {
        return http_reply(pcb, client, "405 Method Not Allowed", "Allow: GET, POST\r\n", "Menetelmää ei tueta.");
    }

    return http_reply(pcb, client, "404 Not Found", "", "Sivua ei löydy.");
}

// Vastaanottaa dataa yhteydestä. Kerää pyynnön puskuriin, kunnes otsakkeet
// (ja Content-Lengthin mukainen runko) ovat perillä, ja välittää sen
// käsiteltäväksi.
static err_t http_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    HttpClient *client = arg;

    // Asiakastietoa ei ole: yhteyttä ei voi palvella.
    if (!client)
    {
        if (p)
        {
            pbuf_free(p);
        }

        tcp_abort(pcb);
        return ERR_ABRT;
    }

    if (err != ERR_OK)
    {
        if (p)
        {
            pbuf_free(p);
        }

        return http_abort_client(pcb, client);
    }

    // p == NULL: vastapuoli sulki yhteyden.
    if (!p)
    {
        if (!client->response_sent || !client->unacknowledged)
        {
            return http_try_close(pcb, client);
        }

        return ERR_OK;
    }

    client->idle_polls = 0;

    // Vastaus on jo lähetetty: ylimääräinen data kuitataan ja hylätään.
    if (client->response_sent)
    {
        tcp_recved(pcb, p->tot_len);
        pbuf_free(p);
        return ERR_OK;
    }

    // Pyyntö ei mahdu puskuriin.
    size_t count = p->tot_len;

    if (count > sizeof(client->request) - 1 - client->used)
    {
        tcp_recved(pcb, p->tot_len);
        pbuf_free(p);
        return http_reply(pcb, client, "413 Content Too Large", "", "Pyyntö liian suuri.");
    }

    pbuf_copy_partial(p, client->request + client->used, (u16_t)count, 0);
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);

    // NUL-tavut rikkoisivat merkkijonokäsittelyn (strstr, sscanf).
    if (memchr(client->request + client->used, 0, count))
    {
        return http_reply(pcb, client, "400 Bad Request", "", "Virheellinen pyyntö.");
    }

    client->used += count;
    client->request[client->used] = 0;

    // Otsakkeet päättyvät tyhjään riviin. Jos sitä ei ole vielä näkynyt,
    // odotetaan lisää dataa.
    char *header_end = strstr(client->request, "\r\n\r\n");

    if (!header_end)
    {
        return ERR_OK;
    }

    return http_handle_request(pcb, client, header_end);
}

// ---------------- Palvelimen käynnistys ----------------

// Uusi yhteys: varataan sille asiakastieto ja asetetaan callbackit.
static err_t http_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
    (void)arg;

    if (err != ERR_OK || !pcb)
    {
        return ERR_VAL;
    }

    HttpClient *client = calloc(1, sizeof(*client));

    if (!client)
    {
        tcp_abort(pcb);
        return ERR_ABRT;
    }

    tcp_arg(pcb, client);
    tcp_recv(pcb, http_recv);
    tcp_sent(pcb, http_sent);
    tcp_err(pcb, http_error);
    tcp_poll(pcb, http_poll, HTTP_POLL_INTERVAL);

    return ERR_OK;
}

bool http_server_start(HttpStatusFn status_fn)
{
    if (!status_fn)
    {
        return false;
    }

    status_callback = status_fn;

    // Asetuslomakkeen tunniste: 128 satunnaista bittiä heksana (32 merkkiä).
    snprintf(settings_token, sizeof(settings_token),
             "%08" PRIx32 "%08" PRIx32 "%08" PRIx32 "%08" PRIx32,
             get_rand_32(), get_rand_32(), get_rand_32(), get_rand_32());

    struct tcp_pcb *pcb = tcp_new();

    if (!pcb)
    {
        return false;
    }

    if (tcp_bind(pcb, IP_ADDR_ANY, HTTP_PORT) != ERR_OK)
    {
        tcp_close(pcb);
        return false;
    }

    // tcp_listen vapauttaa alkuperäisen PCB:n onnistuessaan.
    http_listener = tcp_listen(pcb);

    if (!http_listener)
    {
        tcp_close(pcb);
        return false;
    }

    tcp_accept(http_listener, http_accept);
    printf("HTTP-palvelin kaynnistetty porttiin %d\n", HTTP_PORT);

    return true;
}

unsigned http_server_take_interval_request(void)
{
    unsigned requested = pending_interval_minutes;
    pending_interval_minutes = 0;
    return requested;
}
