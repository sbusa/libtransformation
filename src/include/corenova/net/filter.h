#ifndef __net_filter_H__
#define __net_filter_H__

#include <corenova/interface.h>

#include <corenova/net/transport.h>
#include <corenova/data/parameters.h>

typedef struct {

    char *access_acl;
    char *protocol_acl;
    
} net_filter_t;

typedef struct {

    boolean_t allow;
    
} net_filter_access_t;

DEFINE_INTERFACE (NetFilter) {

    net_filter_t        *(*new)            (parameters_t *params);
    net_filter_access_t *(*filterAccess)   (net_filter_t *, transport_t *);
//    net_filter_access_t *(*filterProtocol) (net_filter_t *, net_analysis_t *);
    void                 (*destroy)        (net_filter_t **);
    
};

#endif
