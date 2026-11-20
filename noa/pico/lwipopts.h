#ifndef _LWIPOPTS_EXAMPLE_COMMONH_H
#define _LWIPOPTS_EXAMPLE_COMMONH_H

// Common settings used in most of the pico_w examples
// (see https://www.nongnu.org/lwip/2_1_x/group__lwip__opts.html for details)

// allow override in some examples
#ifndef NO_SYS
#define NO_SYS 1
#endif
// allow override in some examples
#ifndef LWIP_SOCKET
#define LWIP_SOCKET 0
#endif
#if PICO_CYW43_ARCH_POLL
#define MEM_LIBC_MALLOC 1
#else
// MEM_LIBC_MALLOC is incompatible with non polling versions
#define MEM_LIBC_MALLOC 0
#endif
#define MEM_ALIGNMENT 4
#ifndef MEM_SIZE
#define MEM_SIZE 4000
#endif
#define MEMP_NUM_TCP_SEG 32
#define MEMP_NUM_ARP_QUEUE 10
#define PBUF_POOL_SIZE 24
#define LWIP_ARP 1
#define LWIP_ETHERNET 1
#define LWIP_ICMP 1
#define LWIP_RAW 1
#define TCP_WND (8 * TCP_MSS)
#define TCP_MSS 1460
#define TCP_SND_BUF (8 * TCP_MSS)
#define TCP_SND_QUEUELEN ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))
#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_NETIF_LINK_CALLBACK 1
#define LWIP_NETIF_HOSTNAME 1
#define LWIP_NETCONN 0
#define MEM_STATS 0
#define SYS_STATS 0
#define MEMP_STATS 0
#define LINK_STATS 0
// #define ETH_PAD_SIZE                2
#define LWIP_CHKSUM_ALGORITHM 3
#define LWIP_DHCP 1
#define LWIP_IPV4 1
#define LWIP_TCP 1
#define LWIP_UDP 1
#define LWIP_DNS 1
#define LWIP_TCP_KEEPALIVE 1
#define LWIP_NETIF_TX_SINGLE_PBUF 1
#define DHCP_DOES_ARP_CHECK 0
#define LWIP_DHCP_DOES_ACD_CHECK 0

#ifndef NDEBUG
#define LWIP_DEBUG 1
#define LWIP_STATS 1
#define LWIP_STATS_DISPLAY 1
#endif

#define ETHARP_DEBUG LWIP_DBG_OFF
#define NETIF_DEBUG LWIP_DBG_OFF
#define PBUF_DEBUG LWIP_DBG_OFF
#define API_LIB_DEBUG LWIP_DBG_OFF
#define API_MSG_DEBUG LWIP_DBG_OFF
#define SOCKETS_DEBUG LWIP_DBG_OFF
#define ICMP_DEBUG LWIP_DBG_OFF
#define INET_DEBUG LWIP_DBG_OFF
#define IP_DEBUG LWIP_DBG_OFF
#define IP_REASS_DEBUG LWIP_DBG_OFF
#define RAW_DEBUG LWIP_DBG_OFF
#define MEM_DEBUG LWIP_DBG_OFF
#define MEMP_DEBUG LWIP_DBG_OFF
#define SYS_DEBUG LWIP_DBG_OFF
#define TCP_DEBUG LWIP_DBG_OFF
#define TCP_INPUT_DEBUG LWIP_DBG_OFF
#define TCP_OUTPUT_DEBUG LWIP_DBG_OFF
#define TCP_RTO_DEBUG LWIP_DBG_OFF
#define TCP_CWND_DEBUG LWIP_DBG_OFF
#define TCP_WND_DEBUG LWIP_DBG_OFF
#define TCP_FR_DEBUG LWIP_DBG_OFF
#define TCP_QLEN_DEBUG LWIP_DBG_OFF
#define TCP_RST_DEBUG LWIP_DBG_OFF
#define UDP_DEBUG LWIP_DBG_OFF
#define TCPIP_DEBUG LWIP_DBG_OFF
#define PPP_DEBUG LWIP_DBG_OFF
#define SLIP_DEBUG LWIP_DBG_OFF
#define DHCP_DEBUG LWIP_DBG_OFF
// NTP OSIO
// If we use SNTP we should increase the number of LWIP system timeouts by one
#define MEMP_NUM_SYS_TIMEOUT (LWIP_NUM_SYS_TIMEOUT_INTERNAL + 1)
#define SNTP_MAX_SERVERS LWIP_DHCP_MAX_NTP_SERVERS
#define SNTP_GET_SERVERS_FROM_DHCP LWIP_DHCP_GET_NTP_SRV
#define SNTP_SERVER_DNS 1
#define SNTP_SERVER_ADDRESS "pool.ntp.org"
// show debug information from the lwIP/SNTP application
#define SNTP_DEBUG LWIP_DBG_ON
#define SNTP_PORT LWIP_IANA_PORT_SNTP
// verify IP addresses and port numbers of received packets
#define SNTP_CHECK_RESPONSE 2

// compensate for packet transmission delay
// do NOT enable this on RP2040 if you are using aon_timer to keep the system
// time, because it only has a 1 second resolution (see CMakeLists.txt)
#ifdef DISABLE_SNTP_COMP_ROUNDTRIP
#define SNTP_COMP_ROUNDTRIP 0
#else
#define SNTP_COMP_ROUNDTRIP 1
#endif

#define SNTP_STARTUP_DELAY 1
#define SNTP_STARTUP_DELAY_FUNC (LWIP_RAND() % 5000)
#define SNTP_RECV_TIMEOUT 15000
// how often to query the NTP servers, in ms (60000 is the minimum permitted by RFC4330)
#define SNTP_UPDATE_DELAY 3600000
#define SNTP_RETRY_TIMEOUT SNTP_RECV_TIMEOUT
#define SNTP_RETRY_TIMEOUT_MAX (SNTP_RETRY_TIMEOUT * 10)
#define SNTP_RETRY_TIMEOUT_EXP 1
#define SNTP_MONITOR_SERVER_REACHABILITY 1

//* configure SNTP to use our callback functions for reading and setting the system time
#define SNTP_SET_SYSTEM_TIME_US(sec, us) ntp_set_time(sec, us)
#define SNTP_GET_SYSTEM_TIME(sec, us) ntp_get_time(&(sec), &(us))

//* declare our callback functions (the implementations are in ntp_system_time.c)
#include "stdint.h"
void ntp_set_time(uint32_t sec, uint32_t us);
void ntp_get_time(uint32_t *sec_ptr, uint32_t *us_ptr);

// TLS OSIO
#undef TCP_WND
#define TCP_WND 32768

#define LWIP_ALTCP 1
#define LWIP_ALTCP_TLS 1
#define LWIP_ALTCP_TLS_MBEDTLS 1

#define LWIP_DEBUG 1
#define ALTCP_MBEDTLS_DEBUG LWIP_DBG_ON
#endif /* __LWIPOPTS_H__ */
