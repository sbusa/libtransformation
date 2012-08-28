#ifndef __net_packet_H__
#define __net_packet_H__

#include <corenova/interface.h>

/*----------------------------------------------------------------*/

#include <sys/types.h>

#if defined (freebsd7)
#include <netinet/in_systm.h>
#endif

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#if defined (solaris2) || defined(freebsd7) || defined(freebsd8)

struct iphdr
{

#if __BYTE_ORDER == __LITTLE_ENDIAN
    unsigned int ihl:4;
    unsigned int version:4;
#elif __BYTE_ORDER == __BIG_ENDIAN
    unsigned int version:4;
    unsigned int ihl:4;
#else
#  error "Please fix <bits/endian.h>"
#endif
    u_int8_t tos;
    u_int16_t tot_len;
    u_int16_t id;
    u_int16_t frag_off;
    u_int8_t ttl;
    u_int8_t protocol;
    u_int16_t check;
    u_int32_t saddr;
    u_int32_t daddr;
    /*The options start here. */
};

#endif

typedef struct {

    int size;
    char *data;
    struct iphdr *iphdr;
    struct tcphdr *tcphdr;
    struct udphdr *udphdr;
    
} net_packet_t;

DEFINE_INTERFACE (NetPacket) {
    
	net_packet_t *(*new)        (char *payload, int size);
    void          (*destroy)    (net_packet_t **);
    
};

#endif
