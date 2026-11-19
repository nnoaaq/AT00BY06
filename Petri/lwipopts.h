#ifndef SAAASEMA_LWIPOPTS_H
#define SAAASEMA_LWIPOPTS_H

#include <stdint.h>

// Ei käyttöjärjestelmää: verkkopinoa ajetaan poll-kutsuilla.
#define NO_SYS 1
#define SYS_LIGHTWEIGHT_PROT 0

#define MEM_ALIGNMENT 4
#define MEM_SIZE 32768

#define MEMP_NUM_TCP_PCB 8
#define MEMP_NUM_TCP_PCB_LISTEN 2
#define MEMP_NUM_TCP_SEG 32
#define MEMP_NUM_SYS_TIMEOUT 16

#define PBUF_POOL_SIZE 24

#define LWIP_IPV4 1
#define LWIP_IPV6 0

#define LWIP_ARP 1
#define LWIP_ETHERNET 1
#define LWIP_ICMP 1

#define LWIP_RAW 1
#define LWIP_UDP 1
#define LWIP_TCP 1

#define LWIP_ALTCP 1
#define LWIP_ALTCP_TLS 1
#define LWIP_ALTCP_TLS_MBEDTLS 1

#define LWIP_DHCP 1
#define LWIP_DNS 1

#define LWIP_NETCONN 0
#define LWIP_SOCKET 0

#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_NETIF_LINK_CALLBACK 1
#define LWIP_NETIF_HOSTNAME 1

#define TCP_MSS 1460
#define TCP_WND (4 * TCP_MSS)
#define TCP_SND_BUF (4 * TCP_MSS)
#define TCP_SND_QUEUELEN \
    ((4 * TCP_SND_BUF + TCP_MSS - 1) / TCP_MSS)

#define LWIP_STATS 0
#define LWIP_CHECKSUM_CTRL_PER_NETIF 0

// SNTP_MAX_SERVERS: käytetään yhtä NTP-palvelinta.
// SNTP_CHECK_RESPONSE: tarkistetaan vastauksen kelvollisuus.
// SNTP_SERVER_DNS: palvelin annetaan nimenä, ei IP-osoitteena.

#define SNTP_MAX_SERVERS 1
#define SNTP_CHECK_RESPONSE 2
#define SNTP_SERVER_DNS 1

// SNTP kutsuu tätä, kun NTP-aika on saatu. Toteutus on tiedostossa ntp_clock.c.
#ifdef __cplusplus
extern "C"
{
#endif

    void saaasema_set_ntp_time(uint32_t unix_seconds);

#ifdef __cplusplus
}
#endif

#define SNTP_SET_SYSTEM_TIME(sec) \
    saaasema_set_ntp_time((uint32_t)(sec))

#endif