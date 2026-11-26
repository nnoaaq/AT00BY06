#ifndef AWS_H
#define AWS_H

#include <stdint.h>
#include <stdbool.h>

void process_and_send_measurements(const char *dht_id, const char *ds_id, bool wifi);

#endif