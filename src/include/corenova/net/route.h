#ifndef __net_route_H__
#define __net_route_H__

#include <corenova/interface.h>
#include <corenova/data/list.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct {

    char iface[10];
    in_addr_t src;
    in_addr_t dst;
    in_addr_t mask;
    in_addr_t gateway;

} route_entry_t;


DEFINE_INTERFACE (Route) {

    boolean_t (*addRoute) (route_entry_t *entry);
    boolean_t (*delRoute) (route_entry_t *entry);
    boolean_t (*addHostRoute) (in_addr_t dst, in_addr_t gw, char *iface);
    boolean_t (*delHostRoute) (in_addr_t dst, in_addr_t gw, char *iface);
    in_addr_t (*getIfIP) (char *iface);
    in_addr_t (*getIfGW) (char *iface);
    list_t *(*cacheLookup) (route_entry_t *entry);

};

#endif
