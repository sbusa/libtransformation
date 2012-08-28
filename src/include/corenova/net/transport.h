#ifndef __transport_H__
#define __transport_H__

#include <corenova/interface.h>

#include <corenova/net/ssl.h>
#include <corenova/net/tcp.h>
#include <corenova/net/udp.h>

typedef struct {

    socket_t *socket;

    char    *shost;
    uint32_t saddr; /* in network byte order */
    uint16_t sport;
    
    char    *dhost;
    uint32_t daddr; /* in network byte order */
    uint16_t dport;
    
} transport_info_t;

typedef struct {

    enum transport_type {
        TRANSPORT_SSL = 1,
        TRANSPORT_TCP,
        TRANSPORT_UDP
    } type;

	union {
		ssl_t *ssl;
		tcp_t *tcp;
        udp_t *udp;
	} connection;

    transport_info_t info;

} transport_t;

typedef enum {
    TRANSPORT_POLLIN = 0,
    TRANSPORT_POLLOUT = 1,
    TRANSPORT_POLLINOUT = 2
} transport_poll_type_t;

#define TRANSPORT_POLL_TIMEOUT 10 /* 10ms timeout */

#define TRANSPORT_CONTINUE 1
#define TRANSPORT_TIMEOUT -1
#define TRANSPORT_FATAL   -2

DEFINE_INTERFACE (Transport) {
	transport_t      *(*new)     (enum transport_type, void *connection);
	void              (*destroy) (transport_t **);
    transport_info_t *(*info)    (transport_t *);
    int               (*poll)    (transport_t *, transport_poll_type_t type, int timeout);
	uint32_t          (*send)    (transport_t *, char  *buf, uint32_t size);
	uint32_t          (*recv)    (transport_t *, char **buf, uint32_t size);
	void              (*forcerawtcp) (transport_t *);
	void              (*setTimeout) (transport_t *, int secs);
	void              (*useRecords) (transport_t *, boolean_t state);
};

#endif
