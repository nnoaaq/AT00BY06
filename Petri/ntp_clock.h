#ifndef NTP_CLOCK_H
#define NTP_CLOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

// NTP-synkronoitu kello ja Suomen paikallisajan muotoilu.
//
// lwIP:n SNTP-asiakas kutsuu saaasema_set_ntp_time()-funktiota (määritelty
// lwipopts.h:ssa makron SNTP_SET_SYSTEM_TIME kautta), kun aika on saatu.
// Sen jälkeen kello kulkee Picon omalla mikrosekuntilaskurilla, joten
// Wi-Fi-yhteyden katkeaminen ei nollaa sitä.

// Hakee nykyisen UTC-ajan. Palauttaa false, jos NTP ei ole vielä synkronoitu.
bool ntp_clock_read_utc(time_t *result);

// Muotoilee tallennetun mittauksen aikaleiman muotoon "pp.kk.vvvv hh:mm"
// Suomen aikaa. Arvo 0 tarkoittaa, ettei aikaa ollut tallennushetkellä.
void ntp_clock_format_saved(uint32_t unix_seconds, char *buffer, size_t size);

// Muotoilee nykyhetken muotoon "hh:mm" Suomen aikaa ("--:--" ilman NTP-aikaa).
void ntp_clock_format_now(char *buffer, size_t size);

#endif
