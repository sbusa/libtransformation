#include <corenova/source-stub.h>

THIS = {
    .name = "test_route",
    .version = "0.0",
    .author = "Alex Burkoff <alex.burkoff@gmail.com>",
    .description = "Test routing table manipulation",
    .requires = LIST("corenova.data.string", "corenova.net.route", "corenova.data.list", "corenova.net.resolve", "corenova.sys.quark", "corenova.data.array")
};

#include <corenova/core.h>
#include <corenova/sys/debug.h>
#include <corenova/net/route.h>
#include <corenova/data/string.h>
#include <corenova/net/tcp.h>
#include <corenova/net/socket.h>
#include <corenova/data/list.h>
#include <corenova/data/string.h>
#include <corenova/net/resolve.h>
#include <corenova/sys/quark.h>
#include <corenova/data/array.h>

#include <sys/types.h>

boolean_t stress_test(void *data) {

    int i = 0;

    for(i = 0; i < 100; i++) {

        I(Route)->addHostRoute(I(Resolve)->name2ip("127.0.0.2"), I(Resolve)->name2ip("127.0.0.1"), NULL);

        route_entry_t dest = { .dst = I(Resolve)->name2ip("127.0.0.2"), .src = 0, .mask = 0xffffffff, .iface = "" };

        list_t *cache = I (Route)->cacheLookup(&dest);

        if(cache) {

            if(I (List)->count(cache)>0) {

                list_item_t *item = I (List)->pop(cache);

                route_entry_t *entry = (route_entry_t *) item->data;

                char *src_host = I(Resolve)->ip2string(entry->src);
                char *dst_host = I(Resolve)->ip2string(entry->dst);

                DEBUGP(DDEBUG, "stress_test", "found cached route from '%s' to '%s' via '%s'", src_host, dst_host, entry->iface);

                free(src_host);
                free(dst_host);

                I (ListItem)->destroy(&item);

            } else {

                DEBUGP(DDEBUG, "stress_test", "entry not found");

            }

            I (List)->clear(cache, TRUE);
            I (List)->destroy(&cache);

        }

        I(Route)->delHostRoute(I(Resolve)->name2ip("127.0.0.2"), I(Resolve)->name2ip("127.0.0.1"), NULL);

    }

    return FALSE;
    
}

#define MAX_CLEARED_SESSIONS 1000

typedef struct {

    char *ifname;

} proxy_interface_t;

typedef struct {

    char dst_iface[10];
    uint32_t if_idx;
    in_addr_t src_host;
    in_addr_t dst_host;
    uint32_t created;

} proxy_balance_sess_t;

typedef struct {

    uint32_t if_cnt;
    uint32_t if_last;
    uint32_t sessionTimeout;

    proxy_interface_t *interfaces;
    array_t *sessions;

} net_proxy_t;

static inline int
__sess_cmp(void *first, void *second) {

    proxy_balance_sess_t *a = (proxy_balance_sess_t *) first;
    proxy_balance_sess_t *b = (proxy_balance_sess_t *) second;

    return a->dst_host == b->dst_host && a->src_host == b->src_host ? 1 : 0;

}

static char*
getInterface(net_proxy_t *proxy, in_addr_t src, in_addr_t dst) {

    static int clear_count = 0;
    char *ifname = NULL;

    char *src_host = I (Resolve)->ip2string(src);
    char *dst_host = I (Resolve)->ip2string(dst);

    if (proxy->if_cnt > 0) {

        // first find if there is a cached route to the destination

        route_entry_t dest = { .dst = dst, .src = src, .mask = 0xffffffff, .iface = "" };

        list_t *cache = I (Route)->cacheLookup(&dest);

        if(cache) {

            if(I (List)->count(cache)>0) {

                list_item_t *item = I (List)->pop(cache);

                route_entry_t *entry = (route_entry_t *) item->data;

                DEBUGP(DDEBUG, "getInterface", "found cached route from '%s' to '%s' via '%s'", src_host, dst_host, entry->iface);

                int i = 0;

                for(i = 0; i < proxy->if_cnt; i++) {

                    if(strcmp(proxy->interfaces[i].ifname, entry->iface) == 0) {

                        ifname = proxy->interfaces[i].ifname;

                    }

                }

                I (ListItem)->destroy(&item);

            }

            I (List)->clear(cache, TRUE);
            I (List)->destroy(&cache);

        }

        // second, check internal sessions

        if (!ifname && I(Array)->count(proxy->sessions) > 0) {

            int i = 0;

            proxy_balance_sess_t key = {.src_host = src, .dst_host = dst};

            MODULE_LOCK();

            i = I(Array)->find(proxy->sessions, &key, 0, __sess_cmp);

            if (i >= 0) {

                proxy_balance_sess_t *entry = (proxy_balance_sess_t *) I(Array)->get(proxy->sessions, i);

                if (time(NULL) - entry->created > proxy->sessionTimeout) {

                    DEBUGP(DDEBUG, "getInterface", "found expired connection from '%s' to '%s' via '%s'", src_host, dst_host, entry->dst_iface);
                    I(Array)->clear(proxy->sessions, i);
                    I (Route)->delHostRoute(dst, I (Route)->getIfGW(ifname), entry->dst_iface);
                    free(entry);

                    clear_count++;

                    if(clear_count > MAX_CLEARED_SESSIONS) {

                        I (Array)->cleanup(proxy->sessions);
                        clear_count = 0;

                    }

                } else {

                    DEBUGP(DDEBUG, "getInterface", "found previous connection from '%s' to '%s' via '%s'", src_host, dst_host, entry->dst_iface);
                    ifname = proxy->interfaces[entry->if_idx].ifname;

                }

            }

            MODULE_UNLOCK();

        }

        // nothing found, allocate new internal session and create a route externally

        if (!ifname) {

            int if_idx = proxy->if_last + 1;

            if (if_idx >= proxy->if_cnt) {

                if_idx = 0;

            }

            proxy->if_last = if_idx;

            ifname = proxy->interfaces[if_idx].ifname;

            proxy_balance_sess_t *entry = malloc(sizeof (proxy_balance_sess_t));

            strcpy(entry->dst_iface, ifname);
            entry->dst_host = dst;
            entry->src_host = src;
            entry->created = time(NULL);
            entry->if_idx = if_idx;

            I (Route)->addHostRoute(dst, I (Route)->getIfGW(ifname), ifname);
            I(Array)->add(proxy->sessions, entry);

            DEBUGP(DDEBUG, "getInterface", "previous connection not found, creating new entry from '%s' to '%s' via '%s'", src_host, dst_host, entry->dst_iface);

        }


    }

    if (src_host) free(src_host);
    if (dst_host) free(dst_host);

    return ifname;

}

boolean_t stress_test2(void *data) {

    int i = 0;
    net_proxy_t *proxy = (net_proxy_t *)data;

    for(i = 0; i < 100; i++) {

        DEBUGP(DINFO, "stress_test2", "getInterface() = %s", getInterface(proxy, I(Resolve)->name2ip("127.0.0.1"), I(Resolve)->name2ip("4.2.2.2")));
        DEBUGP(DINFO, "stress_test2", "getInterface() = %s", getInterface(proxy, I(Resolve)->name2ip("127.0.0.1"), I(Resolve)->name2ip("8.8.8.8")));

    }

    return FALSE;

}

int main(int argc, char **argv, char **envp) {

    DebugLevel = 6;

    DEBUGP(DINFO, "main", "IP = %s", I(Resolve)->ip2string(I(Route)->getIfIP("eth0")));
    DEBUGP(DINFO, "main", "GW = %s", I(Resolve)->ip2string(I(Route)->getIfGW("eth0")));
    DEBUGP(DINFO, "main", "IP = %s", I(Resolve)->ip2string(I(Route)->getIfIP("eth1")));
    DEBUGP(DINFO, "main", "GW = %s", I(Resolve)->ip2string(I(Route)->getIfGW("eth1")));

    route_entry_t *dest = (route_entry_t *) calloc(1, sizeof (route_entry_t));

    dest->dst = I(Resolve)->name2ip("127.0.0.1");

    list_t *cache = I(Route)->cacheLookup(dest);
    list_item_t *entry = NULL;

    if (cache) {

        while ((entry = I(List)->pop(cache))) {

            route_entry_t *r = (route_entry_t *) entry->data;

            DEBUGP(DINFO, "main", "SRC = %s, DST = %s, GW = %s, DEV = %s", I(Resolve)->ip2string(r->src), I(Resolve)->ip2string(r->dst), I(Resolve)->ip2string(r->gateway), r->iface);

        }

    }

    DEBUGP(DINFO, "main", "about to run stress test. Press Ctrl+C to abort.");

    sleep(3);

    quark_t *q[100];
    int i = 0;

    net_proxy_t *proxy = calloc(1, sizeof(net_proxy_t));

    proxy->if_cnt = 1;
    proxy->sessionTimeout = 60;

    proxy->sessions = I (Array)->new();
    proxy->interfaces = calloc(2, sizeof(proxy_interface_t));

    proxy->interfaces[0].ifname = strdup("eth0");
    proxy->interfaces[1].ifname = strdup("eth1");

    for(i = 0; i < 100; i++) {

        char name[20];

        sprintf(name, "q%u", i);

        q[i] = I (Quark)->new(stress_test2, proxy);
        I (Quark)->setname(q[i], name);
        I (Quark)->spin(q[i]);

    }

    for(i = 0; i < 100; i++) {

        pthread_join(q[i]->life, NULL);

    }

    return 0;

}

