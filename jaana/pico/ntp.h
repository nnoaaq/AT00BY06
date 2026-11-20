#ifndef NTP_H
#define NTP_H
#include <time.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"


extern time_t current_epoch_time;

typedef struct NTP_T_ {
    ip_addr_t ntp_server_address;
    struct udp_pcb *ntp_pcb;
    async_at_time_worker_t request_worker;
    async_at_time_worker_t resend_worker;
} NTP_T;

#define NTP_SERVER "pool.ntp.org"
#define NTP_MSG_LEN 48
#define NTP_PORT 123
#define NTP_DELTA 2208988800UL // seconds between 1 Jan 1900 and 1 Jan 1970
#define NTP_TEST_TIME_MS (30 * 1000)
#define NTP_RESEND_TIME_MS (10 * 1000)

void run_ntp_test(void);

NTP_T* ntp_init(void);
void ntp_start(NTP_T *state);
void ntp_deinit(NTP_T *state);

#endif