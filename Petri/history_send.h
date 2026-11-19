#ifndef HISTORY_SEND_H
#define HISTORY_SEND_H

#include <stdbool.h>

// Tallennetun historian lähetys AWS:ään yksi mittaus kerrallaan.
//
// Koko historia lähetetään uudelleen aina, kun lähetys käynnistetään
// (Wi-Fi-yhteyden muodostuttua ja jokaisen tallennuksen jälkeen).
// Palvelimen oletetaan tunnistavan jo vastaanotetut mittaukset aikaleiman
// perusteella.

// Sallii tai estää lähetyksen (sauna-tilassa estetty).
void history_send_set_enabled(bool enabled);

// Aloittaa historian lähetyksen alusta. Ei tee mitään, jos lähetys on estetty.
void history_send_start(void);

// Kutsutaan pääsilmukasta. Lähettää seuraavan mittauksen, kun edellinen
// pyyntö on valmis.
void history_send_service(void);

#endif
