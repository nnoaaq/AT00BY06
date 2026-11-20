#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/apps/sntp.h"
#include "pico/util/datetime.h"
#include "pico/aon_timer.h"
#include "pico/mutex.h"

// MUTEX JOTTA EI LUETA AON_TIMERIÄ KUN NTP PÄIVITTÄÄ SITÄ
auto_init_mutex(aon_timer_mutex);
static bool aon_timer_is_initialised = false;

// CALLBACK JOKA ASETTAA AON_TIMERIN NTP VASTAUKSEEN
void ntp_set_time(uint32_t sec, uint32_t us)
{
    static struct timespec ntp_ts;
    ntp_ts.tv_sec = sec;
    ntp_ts.tv_nsec = us * 1000;
    if (aon_timer_is_initialised)
    {
        // AON_TIMER ON ALUSTETTU
        // LUKITAAN SE PÄIVITYKSEN AJAKSI MUTEXILLA
        if (mutex_enter_timeout_ms(&aon_timer_mutex, 10))
        {
            // VARAUS OK
            aon_timer_set_time(&ntp_ts);
            mutex_exit(&aon_timer_mutex); // POISTETAAN VARAUS
            printf("Kello päivitetty.\n");
        }
        else
        {
            printf("Kello oli varattu. Uutta aikaa ei päivitetty.\n");
        }
    }
    else
    {
        // AON_TIMER EI ALUSTETTU
        aon_timer_is_initialised = aon_timer_start(&ntp_ts); // KÄYNNISTETÄÄN KELLO NTP-VASTAUKSEN AJASTA
        printf("Kello käynnistetty.\n");
    }
}

// CALLBACK JONKA AVULLA LWIP/SNTP LUKEE AON_TIMERIA
void ntp_get_time(uint32_t *sec_ptr, uint32_t *us_ptr)
{
    static struct timespec sys_ts;
    aon_timer_get_time(&sys_ts);
    *sec_ptr = sys_ts.tv_sec;
    *us_ptr = sys_ts.tv_nsec / 1000;
}

// FUNKTIO JOKA PALAUTTAA AJAN (ITSE KUTSUTTAVA)
int get_time(struct timespec *ts_ptr)
{
    int retval = 1;
    if (aon_timer_is_initialised)
    {
        if (mutex_enter_timeout_ms(&aon_timer_mutex, 10))
        {
            aon_timer_get_time(ts_ptr);
            mutex_exit(&aon_timer_mutex);
            retval = 0;
        }
    }
    return retval;
}

void ntp_init()
{
    // ALUSTETAAN NTP
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_init();
    setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
}