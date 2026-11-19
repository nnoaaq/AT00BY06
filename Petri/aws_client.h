#ifndef AWS_CLIENT_H
#define AWS_CLIENT_H
#include <stdbool.h>
#include <stdint.h>
bool aws_submit_measurement(
    float temperature,
    float humidity,
    uint32_t measurement_timestamp,
    uint32_t current_timestamp);
bool aws_client_last_send_ok(void);
void aws_client_poll(void);
bool aws_client_busy(void);
#endif
