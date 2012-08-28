#ifndef __tcp_H__
#define __tcp_H__

#include <corenova/interface.h>

#include <corenova/net/socket.h>
#include <netinet/in.h>

typedef struct {
	socket_t *sock;

    boolean_t records;
    
	char              *destHostName;
	struct sockaddr_in destHostAddr;
	uint16_t           destHostPort;

	char              *srcHostName;
	struct sockaddr_in srcHostAddr;
	uint16_t           srcHostPort;
	
	unsigned char retryCounter;
    
} tcp_t;

#define TCP_CONNECT_TIMEOUT   10 /* wait 10 seconds */
#define TCP_CONNECT_MAX_RETRY 3
#define TCP_ACCEPT_MAX_RETRY  3

DEFINE_INTERFACE (TcpConnector) {

    void       (*destroy)    (tcp_t **);
	tcp_t     *(*connect)    (const char *host, u_int16_t port);
	tcp_t     *(*connect2)    (const char *host, u_int16_t port, const char *ifname);
	socket_t  *(*listen)     (u_int16_t port);
	tcp_t     *(*accept)     (socket_t *);

    void       (*useRecords) (tcp_t *, boolean_t state);
	boolean_t  (*setTimeout) (tcp_t *, int32_t seconds);

    uint32_t   (*read)    (tcp_t *, char **buf, uint32_t size);
    uint32_t   (*write)   (tcp_t *, char  *buf, uint32_t size);

};

#endif
