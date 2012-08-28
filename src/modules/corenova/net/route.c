#include <corenova/source-stub.h>

THIS = {
    .version = "0.0",
    .author = "Alex Burkoff <alex.burkoff@gmail.com",
    .description = "This module provides API for routing information access.",
    .requires = LIST("corenova.data.list", "corenova.data.string"),
    .implements = LIST("Route")
};

#include <corenova/net/route.h>
#include <corenova/data/list.h>
#include <corenova/data/string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>

#ifdef freebsd8
#include <net/if_dl.h>
#else
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#endif

/*//////// MODULE CODE //////////////////////////////////////////*/

static boolean_t addRoute(route_entry_t *entry) {

    static int skfd = -1;

#if !defined(freebsd8)
    struct rtentry rt;
#else
    struct ortentry rt;
#endif


    if (entry) {

        memset((char *) &rt, 0, sizeof (rt));

        rt.rt_flags |= RTF_UP;

        if (entry->mask == 0xffffffff) {

            rt.rt_flags |= RTF_HOST;

        }

#if !defined(freebsd8)

        rt.rt_metric = 0;

        ((struct sockaddr_in *) &rt.rt_genmask)->sin_addr.s_addr = entry->mask;
        ((struct sockaddr_in *) &rt.rt_genmask)->sin_family = AF_INET;

        if (entry->iface[0] != '\0') {

            rt.rt_dev = entry->iface;

        }

#else


#endif

        ((struct sockaddr_in *) &rt.rt_dst)->sin_addr.s_addr = entry->dst;
        ((struct sockaddr_in *) &rt.rt_dst)->sin_family = AF_INET;

        ((struct sockaddr_in *) &rt.rt_gateway)->sin_addr.s_addr = entry->gateway;
        ((struct sockaddr_in *) &rt.rt_gateway)->sin_family = AF_INET;

        rt.rt_flags |= RTF_GATEWAY;

#if !defined(freebsd8)

        if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
#else
        if ((skfd = socket(PF_ROUTE, SOCK_RAW, 0)) < 0) {
#endif

            DEBUGP(DERROR, "addRoute", "socket: %s", strerror(errno));
            return FALSE;

        }

#if !defined(freebsd8)

        if (ioctl(skfd, SIOCADDRT, &rt) < 0) {

            DEBUGP(DERROR, "addRoute", "ioctl: %s", strerror(errno));
            close(skfd);
            return FALSE;

        }

#else

        struct {
            struct rt_msghdr rtm;
            struct sockaddr addrs[RTAX_MAX];
        } r;

        memset(&r, 0, sizeof (r));

        r.rtm.rtm_version = RTM_VERSION;
        r.rtm.rtm_type = RTM_ADD;
        r.rtm.rtm_pid = getpid();
        r.rtm.rtm_seq = 0;

        struct sockaddr_in dst = {.sin_addr.s_addr = entry->dst, .sin_family = AF_INET, .sin_len = sizeof (struct sockaddr_in)};
        struct sockaddr_in gw = {.sin_addr.s_addr = entry->gateway, .sin_family = AF_INET, .sin_len = sizeof (struct sockaddr_in)};
        struct sockaddr_in mask = {.sin_addr.s_addr = entry->mask, .sin_family = AF_INET, .sin_len = sizeof (struct sockaddr_in)};


        memcpy(&r.addrs[RTAX_DST], &dst, dst.sin_len);
        memcpy(&r.addrs[RTAX_GATEWAY], &gw, gw.sin_len);
        memcpy(&r.addrs[RTAX_NETMASK], &mask, mask.sin_len);

        r.rtm.rtm_addrs = RTA_DST | RTA_GATEWAY;
        r.rtm.rtm_flags = RTF_STATIC | RTF_GATEWAY;
        r.rtm.rtm_msglen = sizeof (r);

        if (entry->mask != 0xffffffff) {

            r.rtm.rtm_addrs |= RTA_NETMASK;

        } else {

            r.rtm.rtm_flags |= (RTF_HOST);

        }

        if (write(skfd, &r, r.rtm.rtm_msglen) != r.rtm.rtm_msglen) {

            DEBUGP(DERROR, "addRoute", "write: %s", strerror(errno));
            close(skfd);
            return FALSE;

        }

#endif

        close(skfd);

        return TRUE;

    }
    return FALSE;

}

static boolean_t delRoute(route_entry_t * entry) {

    static int skfd = -1;

#if !defined(freebsd8)
    struct rtentry rt;
#else
    struct ortentry rt;
#endif


    if (entry) {

        memset((char *) &rt, 0, sizeof (rt));

        rt.rt_flags |= RTF_UP;

        if (entry->mask == 0xffffffff) {

            rt.rt_flags |= RTF_HOST;

        }

#if !defined(freebsd8)

        rt.rt_metric = 0;

        ((struct sockaddr_in *) &rt.rt_genmask)->sin_addr.s_addr = entry->mask;
        ((struct sockaddr_in *) &rt.rt_genmask)->sin_family = AF_INET;

        if (entry->iface[0] != '\0') {

            rt.rt_dev = entry->iface;

        }

#else


#endif

        ((struct sockaddr_in *) &rt.rt_dst)->sin_addr.s_addr = entry->dst;
        ((struct sockaddr_in *) &rt.rt_dst)->sin_family = AF_INET;

        ((struct sockaddr_in *) &rt.rt_gateway)->sin_addr.s_addr = entry->gateway;
        ((struct sockaddr_in *) &rt.rt_gateway)->sin_family = AF_INET;

        rt.rt_flags |= RTF_GATEWAY;

#if !defined(freebsd8)

        if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
#else
        if ((skfd = socket(PF_ROUTE, SOCK_RAW, 0)) < 0) {
#endif

            DEBUGP(DERROR, "delRoute", "socket: %s", strerror(errno));
            return FALSE;

        }

#if !defined(freebsd8)

        if (ioctl(skfd, SIOCDELRT, &rt) < 0) {

            DEBUGP(DERROR, "delRoute", "ioctl: %s", strerror(errno));
            close(skfd);
            return FALSE;

        }

#else

        struct {
            struct rt_msghdr rtm;
            struct sockaddr addrs[RTAX_MAX];
        } r;

        memset(&r, 0, sizeof (r));

        r.rtm.rtm_version = RTM_VERSION;
        r.rtm.rtm_type = RTM_DELETE;
        r.rtm.rtm_pid = getpid();
        r.rtm.rtm_seq = 0;

        struct sockaddr_in dst = {.sin_addr.s_addr = entry->dst, .sin_family = AF_INET, .sin_len = sizeof (struct sockaddr_in)};
        struct sockaddr_in gw = {.sin_addr.s_addr = entry->gateway, .sin_family = AF_INET, .sin_len = sizeof (struct sockaddr_in)};
        struct sockaddr_in mask = {.sin_addr.s_addr = entry->mask, .sin_family = AF_INET, .sin_len = sizeof (struct sockaddr_in)};


        memcpy(&r.addrs[RTAX_DST], &dst, dst.sin_len);
        memcpy(&r.addrs[RTAX_GATEWAY], &gw, gw.sin_len);
        memcpy(&r.addrs[RTAX_NETMASK], &mask, mask.sin_len);

        r.rtm.rtm_addrs = RTA_DST | RTA_GATEWAY;
        r.rtm.rtm_flags = RTF_DONE;
        r.rtm.rtm_msglen = sizeof (r);

        if (entry->mask != 0xffffffff) {

            r.rtm.rtm_addrs |= RTA_NETMASK;

        } else {

            r.rtm.rtm_flags |= (RTF_HOST);

        }

        if (write(skfd, &r, r.rtm.rtm_msglen) != r.rtm.rtm_msglen) {

            DEBUGP(DERROR, "delRoute", "write: %s", strerror(errno));
            close(skfd);
            return FALSE;

        }

#endif

        close(skfd);

        return TRUE;

    }
    return FALSE;

}

static list_t *routeLookup(route_entry_t *key) {

    list_t *result = NULL;
    int skfd = -1;

    if (key) {

#ifdef freebsd8

        struct ortentry rt;

        memset((char *) &rt, 0, sizeof (rt));

        rt.rt_flags |= RTF_UP;

        if (key->mask == 0xffffffff) {

            rt.rt_flags |= RTF_HOST;

        }

        rt.rt_flags |= RTF_GATEWAY;

        if ((skfd = socket(PF_ROUTE, SOCK_RAW, 0)) < 0) {

            DEBUGP(DERROR, "routeLookup", "socket: %s", strerror(errno));
            return result;

        }

        struct {
            struct rt_msghdr rtm;
            struct sockaddr addrs[RTAX_MAX];
        } r;

        memset(&r, 0, sizeof (r));

        r.rtm.rtm_version = RTM_VERSION;
        r.rtm.rtm_type = RTM_GET;
        r.rtm.rtm_pid = getpid();
        r.rtm.rtm_seq = 0;

        struct sockaddr_in dst = {.sin_addr.s_addr = key->dst, .sin_family = AF_INET, .sin_len = sizeof (struct sockaddr_in)};
        struct sockaddr_in mask = {.sin_addr.s_addr = key->mask, .sin_family = AF_INET, .sin_len = sizeof (struct sockaddr_in)};


        memcpy(&r.addrs[RTAX_DST], &dst, dst.sin_len);
        memcpy(&r.addrs[RTAX_NETMASK], &mask, mask.sin_len);

        r.rtm.rtm_addrs = RTA_DST;
        r.rtm.rtm_flags = RTF_DONE;
        r.rtm.rtm_msglen = sizeof (r);

        if (key->mask != 0xffffffff) {

            r.rtm.rtm_addrs |= RTA_NETMASK;

        } else {

            r.rtm.rtm_flags |= (RTF_HOST);

        }

        if (write(skfd, &r, r.rtm.rtm_msglen) != r.rtm.rtm_msglen) {

            DEBUGP(DERROR, "routeLookup", "write: %s", strerror(errno));
            close(skfd);
            return result;

        }

        result = I(List)->new();

        while (1) {


            if (read(skfd, (struct rt_msghdr *) &r, sizeof (r)) < 0) {

                DEBUGP(DERROR, "routeLookup", "read: %s", strerror(errno));
                close(skfd);
                break;

            }

            route_entry_t *e = calloc(1, sizeof (route_entry_t));

            e->gateway = ((struct sockaddr_in*) &r.addrs[RTAX_GATEWAY])->sin_addr.s_addr;

            I(List)->insert(result, I(ListItem)->new(e));

            if (r.rtm.rtm_flags & RTF_DONE) {

                break;

            }

        }

#endif

#ifdef linux

        struct {
            struct nlmsghdr n;
            struct rtmsg r;
            char buf[1024];
        } req;

        struct nhop {
            in_addr_t gw;
            char dev[IFNAMSIZ];
        };

        list_t *nhops = NULL;

        struct nlmsghdr *h;
        struct rtmsg *rtmp;
        struct rtattr *rtatp;
        int rtattrlen;
        int rval = -1;
        char buf[4096];
        char dev[IFNAMSIZ];
        in_addr_t src = 0;
        in_addr_t dst = 0;
        in_addr_t mask = 0;
        in_addr_t gw = 0;

        if ((skfd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE)) < 0) {

            DEBUGP(DERROR, "routeLookup", "socket: %s", strerror(errno));
            return FALSE;

        }

        memset(&req, 0, sizeof (req));
        req.n.nlmsg_len = NLMSG_LENGTH(sizeof (struct rtmsg));
        req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
        req.n.nlmsg_type = RTM_GETROUTE;
        req.n.nlmsg_len = NLMSG_ALIGN(req.n.nlmsg_len);

        if (send(skfd, &req, req.n.nlmsg_len, 0) < 0) {

            DEBUGP(DERROR, "routeLookup", "send: %s", strerror(errno));
            close(skfd);
            return result;

        }

        if ((rval = recv(skfd, buf, sizeof (buf), 0)) < 0) {

            DEBUGP(DERROR, "routeLookup", "recv: %s", strerror(errno));
            close(skfd);
            return result;

        }

        for (h = (struct nlmsghdr *) buf; NLMSG_OK(h, rval); h = NLMSG_NEXT(h, rval)) {

            rtmp = (struct rtmsg *) NLMSG_DATA(h);
            rtatp = (struct rtattr *) RTM_RTA(rtmp);
            rtattrlen = RTM_PAYLOAD(h);

            src = 0;
            dst = 0;
            mask = 0;
            gw = 0;


            if (result == NULL) {

                result = I(List)->new();

            }

            for (; RTA_OK(rtatp, rtattrlen); rtatp = RTA_NEXT(rtatp, rtattrlen)) {

                switch (rtatp->rta_type) {

                    case RTA_OIF:
                    {
                        int oif_index = *(int *) RTA_DATA(rtatp);

                        if_indextoname(oif_index, dev);
                        break;
                    }

                    case RTA_PREFSRC: src = *((in_addr_t *) RTA_DATA(rtatp));
                        break;
                    case RTA_DST: dst = *((in_addr_t *) RTA_DATA(rtatp));
                        mask = rtmp->rtm_dst_len != 0 ? htonl(~0 << (32 - rtmp->rtm_dst_len)) : 0;
                        break;
                    case RTA_GATEWAY: gw = *((in_addr_t *) RTA_DATA(rtatp));
                        break;

                    case RTA_MULTIPATH:
                    {

                        nhops = I(List)->new();

                        struct rtnexthop *nh = NULL;
                        
                        struct rtattr *nhrtattr = NULL;
                        int nh_len = RTA_PAYLOAD(rtatp);

                        for (nh = RTA_DATA(rtatp); RTNH_OK(nh, nh_len); nh = RTNH_NEXT(nh)) {

                            struct nhop *hop = calloc(1, sizeof (struct nhop));

                            int attr_len = nh->rtnh_len - sizeof (*nh);

                            if (nh_len < sizeof (*nh))
                                break;
                            if (nh->rtnh_len > nh_len)
                                break;

                            if (nh->rtnh_len > sizeof (*nh)) {

                                if_indextoname(nh->rtnh_ifindex, hop->dev);

                                for (nhrtattr = RTNH_DATA(nh); RTA_OK(nhrtattr, attr_len); nhrtattr = RTA_NEXT(nhrtattr, attr_len)) {

                                    switch (nhrtattr->rta_type) {

                                        case RTA_GATEWAY:
                                            hop->gw = *((in_addr_t *) RTA_DATA(nhrtattr));
                                            break;


                                    }

                                }

                                I(List)->insert(nhops, I(ListItem)->new(hop));

                            }

                            nh_len -= NLMSG_ALIGN(nh->rtnh_len);
                        }

                        break;
                    }

                }

            }

            if (nhops == NULL) {

                if (key && ((key->dst != dst) || (key->iface[0] != '\0' && strcmp(key->iface, dev) != 0))) {

                    continue;

                }

                route_entry_t *r = calloc(1, sizeof (route_entry_t));

                r->gateway = gw;
                r->mask = mask;
                r->dst = dst;
                r->src = src;
                strcpy(r->iface, dev);

                I(List)->insert(result, I(ListItem)->new(r));

            } else {

                struct nhop *h = NULL;
                list_item_t *item = NULL;

                while (item = I(List)->pop(nhops)) {

                    h = item->data;

                    if (key && ((key->dst != dst) || (key->iface[0] != '\0' && strcmp(key->iface, h->dev) != 0))) {

                        I(ListItem)->destroy(&item);
                        free(h);
                        continue;

                    }

                    route_entry_t *r = calloc(1, sizeof (route_entry_t));

                    r->gateway = h->gw;
                    r->mask = mask;
                    r->dst = dst;
                    r->src = src;
                    strcpy(r->iface, h->dev);

                    I(List)->insert(result, I(ListItem)->new(r));
                    I(ListItem)->destroy(&item);
                    free(h);

                }

                I(List)->destroy(&nhops);

            }

        }

#endif

        close(skfd);

        return result;

    }

    return result;

}

static list_t * cacheLookup(route_entry_t * dest) {

    list_t *result = NULL;

#if defined(linux)

    FILE *f = fopen("/proc/net/rt_cache", "r");

    if (f) {

        char buf[512];

        result = I(List)->new();

        fgets(buf, sizeof (buf), f); // skip header

        while (!feof(f)) {

            if (fgets(buf, sizeof (buf), f)) {

                list_t *fields = I(String)->tokenize(buf, "\t");

                int i = 0;
                list_item_t *item;

                route_entry_t *entry = calloc(1, sizeof (route_entry_t));

                while ((item = I(List)->pop(fields))) {

                    switch (i) {

                        case 0: strcpy(entry->iface, (char*) item->data);
                            break;
                        case 1: sscanf((char*) item->data, "%X", &entry->dst);
                            break;
                        case 2: sscanf((char*) item->data, "%X", &entry->gateway);
                            break;
                        case 7: sscanf((char*) item->data, "%X", &entry->src);
                            break;

                    }

                    free(item->data);
                    I(ListItem)->destroy(&item);
                    i++;

                }

                I(List)->destroy(&fields);

                if (dest) {

                    if (dest->dst && dest->dst != entry->dst) {

                        free(entry);
                        continue;

                    }

                }

                I(List)->insert(result, I(ListItem)->new(entry));


            }

        }

        fclose(f);

    }

#endif

    return result;

}

static boolean_t addHostRoute(in_addr_t dst, in_addr_t gw, char *iface) {

    route_entry_t route = {.dst = dst, .src = 0, .gateway = gw, .mask = 0xffffffff};
    boolean_t result = FALSE;

    if (iface)
        strcpy(route.iface, iface);
    else
        route.iface[0] = '\0';

    result = addRoute(&route);

    return result;

}

static boolean_t delHostRoute(in_addr_t dst, in_addr_t gw, char *iface) {

    route_entry_t route = {.dst = dst, .src = 0, .gateway = gw, .mask = 0xffffffff};
    boolean_t result = FALSE;

    if (iface)
        strcpy(route.iface, iface);
    else
        route.iface[0] = '\0';

    result = delRoute(&route);

    return result;

}

static in_addr_t getIfIP(char *iface) {

    in_addr_t result = 0;

    if (iface) {

        struct ifreq req;
        int sock = socket(AF_INET, SOCK_DGRAM, 0);

        memset(&req, 0, sizeof (struct ifreq));
        strcpy(req.ifr_name, iface);

        if (ioctl(sock, SIOCGIFADDR, &req) >= 0) {

            result = ((struct sockaddr_in *) &req.ifr_addr)->sin_addr.s_addr;

        } else {

            DEBUGP(DERROR, "getIfIP", "ioctl: %s", strerror(errno));

        }

        close(sock);

    }

    return result;

}

static in_addr_t getIfGW(char *iface) {

    route_entry_t route = {.dst = 0, .src = 0, .gateway = 0, .mask = 0};
    list_t *routes = NULL;
    in_addr_t result = 0;

    if (iface)
        strcpy(route.iface, iface);
    else
        route.iface[0] = '\0';

    routes = routeLookup(&route);

    if (routes) {

        if (I(List)->count(routes) > 0) {

            list_item_t *e = I(List)->pop(routes);
            route_entry_t *entry = (route_entry_t *) e->data;

            result = entry->gateway;

            free(entry);
            I(ListItem)->destroy(&e);

        }

        I(List)->clear(routes, TRUE);
        I(List)->destroy(&routes);

    }

    return result;

}

IMPLEMENT_INTERFACE(Route) = {

    .addRoute = addRoute,
    .delRoute = delRoute,
    .cacheLookup = cacheLookup,
    .addHostRoute = addHostRoute,
    .delHostRoute = delHostRoute,
    .getIfGW = getIfGW,
    .getIfIP = getIfIP

};
