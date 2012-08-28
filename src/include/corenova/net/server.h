#ifndef __net_server_H__
#define __net_server_H__

#include <corenova/interface.h>

#include <corenova/net/transport.h>
#include <corenova/data/parameters.h>

typedef struct {

    int listen_port;
    socket_t *listenSocket;

    boolean_t use_ssl;
    ssl_context_t *ctx;
    
} net_server_t;

DEFINE_INTERFACE (NetServer) {

    net_server_t *(*new)     (parameters_t *params);
    boolean_t     (*listen)  (net_server_t *);
    transport_t  *(*accept)  (net_server_t *);
    void          (*destroy) (net_server_t **);
    
};

#endif
