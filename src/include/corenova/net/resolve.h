#ifndef __net_resolve_H__
#define __net_resolve_H__

#include <corenova/macros.h>
#include <corenova/interface.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>			/* for inet_ntoa */

DEFINE_INTERFACE (Resolve) {

    in_addr_t (*name2ip)     (const char *h);
    char     *(*ip2string)   (in_addr_t ip);
    
};

typedef struct {

    char *host;
    int   port;
    
} host_port_t;

DEFINE_INTERFACE (HostPort) {
    
    host_port_t *(*new)     (const char *);
    void         (*destroy) (host_port_t **);
    
};

#endif
