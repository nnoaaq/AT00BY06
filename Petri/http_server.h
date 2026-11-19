#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stdbool.h>
#include <stdint.h>

// Pieni HTTP-palvelin (portti 80): tilasivu ja tallennusvälin asetus.
//
//   GET  /          tilasivu, päivittyy 10 sekunnin välein
//   POST /settings  tallennusvälin valinta (1, 15 tai 60 min)
//
// Asetuslomake sisältää satunnaisen tunnisteen (token), jonka palvelin luo
// käynnistyessään. Se estää, ettei toinen verkkosivu voi vaihtaa asetusta
// käyttäjän selaimen kautta (CSRF). Asetus ei tallennu flashiin, vaan
// uudelleenkäynnistyksen jälkeen palataan oletukseen.

// Tilasivulla näytettävät tiedot.
typedef struct
{
    bool measurement_valid;      // onko minuuttikeskiarvo saatavilla
    float temperature;           // °C
    float humidity;              // %RH
    float internal_temperature;  // Picon sisäinen lämpötila, °C
    uint8_t battery_percent;
    char measurement_time[16];   // "hh:mm" tai "--:--"
    uint32_t stored_count;       // mittauksia historiassa
    unsigned interval_minutes;   // nykyinen tallennusjakso
    unsigned seconds_to_period_end; // aikaa jakson päättymiseen
} HttpStatus;

// Palvelin pyytää tämän funktion kautta tuoreet tiedot jokaisella
// sivulatauksella, joten pääohjelman ei tarvitse päivittää niitä erikseen.
typedef void (*HttpStatusFn)(HttpStatus *status);

// Käynnistää palvelimen. Palauttaa false, jos portin avaus epäonnistui.
bool http_server_start(HttpStatusFn status_fn);

// Palauttaa selaimesta pyydetyn uuden tallennusvälin (1, 15 tai 60) tai
// 0, jos uutta pyyntöä ei ole. Pyyntö nollataan lukemisen yhteydessä.
unsigned http_server_take_interval_request(void);

#endif
