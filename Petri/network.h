#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>

#include "http_server.h"

// Wi-Fi-yhteyden hallinta: yhdistäminen, uudelleenyritykset, SNTP ja
// HTTP-palvelimen käynnistys.

// Kutsutaan joka kerta, kun Wi-Fi-yhteys on muodostunut.
typedef void (*NetworkConnectedFn)(void);

// Alustaa Wi-Fi-piirin. Palauttaa false, jos alustus epäonnistui; silloin
// laite jatkaa toimintaansa ilman verkkoa.
// status_fn välitetään HTTP-palvelimelle, on_connected voi olla NULL.
bool network_init(HttpStatusFn status_fn, NetworkConnectedFn on_connected);

// Kutsutaan pääsilmukasta: ajaa Wi-Fi-ajurin ja yhteyden tilakoneen.
void network_service(void);

// Pysäyttää (true) tai palauttaa (false) Wi-Fi-yhteyden. Pysäytys katkaisee
// yhteyden; HTTP-palvelin jää kuuntelemaan, mutta ei ole tavoitettavissa,
// ennen kuin yhteys palaa.
void network_set_paused(bool paused);

#endif
