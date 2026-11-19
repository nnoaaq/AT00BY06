#ifndef AVERAGE_H
#define AVERAGE_H

#include <stdint.h>

// Keskiarvolaskuri: näytteet summataan ja jaetaan lopuksi näytteiden määrällä.
// Näytön (1 min) ja tallennuksen (1/15/60 min) keskiarvoille on omat,
// toisistaan riippumattomat laskurit.
typedef struct
{
    double temperature;
    double humidity;
    double internal;
    uint32_t count;
} Average;

static inline void average_add(Average *a, float t, float h, float internal)
{
    a->temperature += t;
    a->humidity += h;
    a->internal += internal;
    a->count++;
}

static inline void average_reset(Average *a)
{
    *a = (Average){0};
}

#endif
