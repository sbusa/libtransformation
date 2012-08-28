#include <corenova/source-stub.h>

THIS = {
    .version = "2.1",
    .author = "Peter K. Lee <saint@corenova.com>",
    .description = "This module provides low level socket support.",
    .requires = LIST("corenova.data.list",
    "corenova.net.resolve"),
    .implements = LIST("Socket", "SocketPair")
};

#include <corenova/net/socket.h>
#include <corenova/net/resolve.h>
#include <corenova/data/list.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <netinet/in.h> /* for htons() */

#if defined (linux)

#include <net/if_arp.h>
#include <linux/if.h>
#include <linux/if_packet.h>
#include <linux/netlink.h>
#include <linux/if_ether.h>

#else

#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>

#define ETH_P_ALL       0x0003          /* Every packet (be careful!!!) */  

struct sockaddr_ll {
    uint16_t sll_family;
    uint16_t sll_protocol;
    int32_t sll_ifindex;
    uint16_t sll_hatype;
    unsigned char sll_pkttype;
    unsigned char sll_halen;
    unsigned char sll_addr[8];
};

#endif

#if defined(freebsd8)

#include <net/if.h>
#include <net/if_dl.h>

#endif

#if defined (solaris2)
#include <sys/sockio.h>
#endif

/*//////// API ROUTINES //////////////////////////////////////////*/

static socket_t *
_new(enum _socketTypes type) {
    socket_t *sock = (socket_t *) calloc(1, sizeof (socket_t));
    if (sock) {
        DEBUGP(DDEBUG, "new", "accessing socket type [%s]", _socketDescr[type]);
        sock->type = type;
        switch (type) {

            case SOCKET_DGRAM:
                sock->skd = socket(AF_INET, SOCK_DGRAM, 0);
                break;


#if defined (linux)

            case SOCKET_NETLINK:
                sock->skd = socket(AF_NETLINK, SOCK_RAW, NETLINK_USERSOCK);
                break;

            case SOCKET_RAW:
                sock->skd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
                break;
#endif
            case SOCKET_STREAM:
                sock->skd = socket(AF_INET, SOCK_STREAM, 0);
                break;

            case SOCKET_DUMMY:
                break;

            case SOCKET_UNIX:
                sock->skd = socket(AF_UNIX, SOCK_STREAM, 0);
                break;

            default:
                sock->skd = -1;
                break;

        }

        if (sock->skd == -1) {

            DEBUGP(DERR, "create", "unable to create socket type [%d]", type);
            free(sock);
            sock = NULL;

        }

    }

    return sock;
}

static void
_destroy(socket_t **sock) {

    if (sock && *sock) {

        if ((*sock)->skd > 0)
            close((*sock)->skd);

        free(*sock);
        *sock = NULL;

    }

}

static boolean_t
_bindInterface(socket_t *sock, const char *ifname) { // freebsd8 code is not correct here
    if (sock && ifname) {
        struct ifreq req;

#if !defined(freebsd8)
        struct sockaddr_ll skaddr;
#else
        struct sockaddr_dl skaddr;
#endif
        // set the interface
        memset(&req, 0, sizeof (struct ifreq));
        strncpy(req.ifr_name, ifname, IFNAMSIZ);
        req.ifr_name[IFNAMSIZ - 1] = '\0';
        // verify that the interface exists
        if (ioctl(sock->skd, SIOCGIFINDEX, &req) >= 0) {
            // bind to the interface
            memset(&skaddr, 0, sizeof (skaddr));
#if defined (linux)
            skaddr.sll_protocol = htons(ETH_P_ALL);
            skaddr.sll_ifindex = req.ifr_ifindex;
            skaddr.sll_family = AF_PACKET;
#elif defined(freebsd8)
            skaddr.sdl_index = req.ifr_index;
            skaddr.sdl_family = AF_LINK;
#else            
            skaddr.sll_family = AF_ROUTE;
#endif			

#if !defined(freebsd8)
            if (bind(sock->skd, (struct sockaddr *) &skaddr, sizeof (struct sockaddr_ll)) >= 0)
#else
            if (bind(sock->skd, (struct sockaddr *) &req.ifr_addr, sizeof (struct sockaddr)) >= 0)
#endif
                return TRUE;
            else {
                DEBUGP(DERR, "bindInterface", "unable to bind socket to %s:%s", ifname, strerror(errno));
            }
        } else {
            DEBUGP(DERR, "bindInterface", "interface %s does not exist.", ifname);
        }
    }
    return FALSE;
}

static boolean_t
_bind(socket_t *sock, struct sockaddr* addr) {

    if (sock && addr) {

        if (bind(sock->skd, (struct sockaddr *) addr, sizeof (struct sockaddr)) >= 0) {

            return TRUE;

        }
        else {

            DEBUGP(DERR, "bind", "unable to bind socket: %s", strerror(errno));

        }
    }

    return FALSE;
}

static boolean_t
_setOpt(socket_t *sock, int opt) {
    int32_t val = 0;
    int res = 0;

    if (sock) {

        switch (opt) {
            case BCAST:
                DEBUGP(DDEBUG, "setOpt", "enabling BROADCAST on socket #%u", sock->skd);
                val = 1;
                res = setsockopt(sock->skd, SOL_SOCKET, SO_BROADCAST, &val, sizeof (val));
                break;
        }

        if (res == 0) return TRUE;

        DEBUGP(DWARN, "setOpt", "setsockopt() failed: %s", strerror(errno));
    }
    return FALSE;
}

static boolean_t
_setFlag(socket_t *sock, const char *ifname, short flag) {
    if (sock) {
        struct ifreq ifr;
        strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
        ifr.ifr_name[IFNAMSIZ - 1] = '\0';
        if (ioctl(sock->skd, SIOCGIFFLAGS, &ifr) >= 0) {
            ifr.ifr_flags |= flag;
            if (ioctl(sock->skd, SIOCSIFFLAGS, &ifr) >= 0)
                return TRUE;
        }
    }
    return FALSE;
}

static boolean_t
_clearFlag(socket_t *sock, const char *ifname, short flag) {
    if (sock) {
        struct ifreq ifr;
        strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
        ifr.ifr_name[IFNAMSIZ - 1] = '\0';
        if (ioctl(sock->skd, SIOCGIFFLAGS, &ifr) >= 0) {
            ifr.ifr_flags &= ~flag;
            if (ioctl(sock->skd, SIOCSIFFLAGS, &ifr) >= 0)
                return TRUE;
        }
    }
    return FALSE;
}

static boolean_t
_verifyArpType(socket_t *sock, const char *ifname, u_int16_t type) {
    if (sock) {
        struct ifreq ifr;
        strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
        ifr.ifr_name[IFNAMSIZ - 1] = '\0';
#if defined (linux)        
        if (ioctl(sock->skd, SIOCGIFHWADDR, &ifr) >= 0) {

            if (ifr.ifr_hwaddr.sa_family == type)
                return TRUE;
        }
#elif defined (freebsd6) || defined (freebsd7)
        if (ioctl(sock->skd, SIOCGIFADDR, &ifr) >= 0) {

            if (ifr.ifr_ifru.ifru_phys == type)
                return TRUE;
        }
#else
        if (ioctl(sock->skd, SIOCGIFADDR, &ifr) >= 0) {
            // XXX - hack for now!!!
            return TRUE;
        }
#endif
        DEBUGP(DERR, "verifyArpType", "%s", strerror(errno));
    }
    return FALSE;
}

static boolean_t
setAddressSocket(socket_t *socket, const char *to, int port) {
    if (socket && to) {
        switch (socket->type) {
            case SOCKET_DGRAM:
            case SOCKET_STREAM:
            {
                in_addr_t ip = I(Resolve)->name2ip(to);
                if (ip != INADDR_NONE) {
                    socket->addr.in.sin_family = AF_INET;
                    socket->addr.in.sin_addr.s_addr = ip;
                    socket->addr.in.sin_port = htons(port);
                    socket->len = sizeof (socket->addr.in);
                    return TRUE;
                } else {
                    DEBUGP(DERR, "setAddress", "unable to resolve %s for setting socket address!", to);
                }
                break;
            }
            case SOCKET_UNIX:
            {
                memset((char *) &socket->addr.un, 0, sizeof (socket->addr.un));
                socket->addr.un.sun_family = AF_UNIX;
                strncpy(socket->addr.un.sun_path, to, sizeof (socket->addr.un));
                socket->len = sizeof (socket->addr.un);
                return TRUE;
            }
            case SOCKET_DUMMY:
            case SOCKET_NETLINK:
            case SOCKET_RAW:
            default:
                DEBUGP(DWARN, "setAddress", "cannot set address for '%s' socket!", _socketDescr[socket->type]);
        }
    }
    return FALSE;
}

IMPLEMENT_INTERFACE(Socket) = {
    .new = _new,
    .destroy = _destroy,
    .bindInterface = _bindInterface,
    .setFlag = _setFlag,
    .clearFlag = _clearFlag,
    .verifyArpType = _verifyArpType,
    .setOpt = _setOpt,
    .setAddress = setAddressSocket,
    .bind = _bind
};

/*//////// SocketPair Interface Implementation //////////////////////////////////////////*/

static socket_pair_t *
newSocketPair(void) {
    socket_pair_t *pair = (socket_pair_t *) calloc(1, sizeof (socket_pair_t));
    if (pair) {
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair->fds) < 0) {
            DEBUGP(DERR, "newSocketPair", "failed to create a new socket pair!");
            free(pair);
            pair = NULL;
        }
    }
    return pair;
}

static void
closeSocketPair(socket_pair_t *pair) {
    if (pair) {
        /* do a soft close until it is explicitly destroyed! */
        DEBUGP(DDEBUG, "closeSocketPair", "a soft close of %d and %d", pair->fds[0], pair->fds[1]);
        shutdown(pair->fds[0], SHUT_RDWR);
        shutdown(pair->fds[1], SHUT_RDWR);
    }
}

static void
destroySocketPair(socket_pair_t **ptr) {
    if (ptr) {
        socket_pair_t *pair = *ptr;
        if (pair) {
            DEBUGP(DDEBUG, "destroySocketPair", "a full close of %d and %d", pair->fds[0], pair->fds[1]);
            close(pair->fds[0]);
            close(pair->fds[1]);
            free(pair);
            *ptr = NULL;
        }
    }
}

IMPLEMENT_INTERFACE(SocketPair) = {

    .new = newSocketPair,
    .close = closeSocketPair,
    .destroy = destroySocketPair

};
