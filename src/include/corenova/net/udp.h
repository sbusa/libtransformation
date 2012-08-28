#ifndef __udp_H__
#define __udp_H__

#include <corenova/macros.h>
#include <corenova/interface.h>

#include <corenova/net/socket.h>
#include <netinet/in.h>

typedef struct {
    socket_t *sock;

    char              *destHostName;
    struct sockaddr_in destHostAddr;
    uint16_t           destHostPort;

    char              *srcHostName;
    struct sockaddr_in srcHostAddr;
    uint16_t           srcHostPort;
	
    unsigned char retryCounter;

} udp_t;

DEFINE_INTERFACE (UdpConnector) {

    void       (*destroy)    (udp_t **);
    udp_t     *(*connect)    (const char *host, u_int16_t port);
    udp_t     *(*bconnect)    (const char *host, u_int16_t port);
    socket_t  *(*listen)     (u_int16_t port);
    socket_t  *(*listen2)     (in_addr_t host, u_int16_t port);

    uint32_t   (*read)    (udp_t *, char **buf, uint32_t size);
    uint32_t   (*write)   (udp_t *, char  *buf, uint32_t size);

};

#endif
