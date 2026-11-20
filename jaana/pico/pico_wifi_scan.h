#ifndef PICO_WIFI_SCAN_H
#define PICO_WIFI_SCAN_H

#include <stdbool.h>
#include "pico/cyw43_arch.h"

#define TARGET_SSID "xxxxxxxx"
#define TARGET_PASSWORD "xxxxxxxx"

// Jaettu tilatieto skannaukselle
typedef struct {
    bool scan_started;
    bool network_found;
} wifi_scan_state_t;

void scan_worker_fn(async_context_t *context, async_at_time_worker_t *worker);

#endif
