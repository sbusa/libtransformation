#include <corenova/source-stub.h>

THIS = {
	.version = "2.0",
	.author  = "Peter K. Lee <saint@corenova.com>",
	.description = "This module provides name resolution and some basic parsing.",
	.implements = LIST ("Resolve","HostPort"),
    .requires = LIST ("corenova.data.string","corenova.data.list")
};

#include <corenova/net/resolve.h>
#include <corenova/data/list.h>
#include <corenova/data/string.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <netdb.h>              /* for hostent and gethostbyname */

/*//////// Resolve Interface Implementation //////////////////////////////////////////*/

/*
 * Optimized to use getaddrinfo () function
 */
static in_addr_t
name2ip (const char *h) {
    struct addrinfo hints, *res;
    in_addr_t ip;
    if (h) {
        if ((ip = inet_addr (h)) != INADDR_NONE)
            return ip;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        if (getaddrinfo (h, NULL, &hints, &res) == 0) {
            struct sockaddr_in *sa = (struct sockaddr_in *) res->ai_addr;
            ip = sa->sin_addr.s_addr;
            freeaddrinfo (res);
        } else
            return INADDR_NONE;

    } else {
        ip = INADDR_NONE;
    }
    return ip;
}

static char *
ip2string (in_addr_t ip) {
    struct in_addr addr = {
        .s_addr = ip
    };
    char *ipstr = NULL;
    MODULE_LOCK ();
    ipstr = inet_ntoa (addr);
    if (ipstr) ipstr = strdup (ipstr);
    MODULE_UNLOCK ();
    return ipstr;
}

IMPLEMENT_INTERFACE (Resolve) = {
    
    .name2ip   = name2ip,
    .ip2string = ip2string
    
};

static host_port_t *
newHostPortPair (const char *addr) {
    if (addr) {
        list_t *tokens = I (String)->tokenize (addr,":");
        if (tokens && I (List)->count (tokens) == 2) {
            list_item_t *item1 = I (List)->pop (tokens), *item2 = I (List)->pop (tokens);
            if (item1 && item2) {
                char *host = (char *) item1->data, *port = (char *) item2->data;
                if (host && port) {
                    host_port_t *target = (host_port_t *)calloc (1,sizeof (host_port_t));
                    if (target) {
                        target->host = I (String)->copy (host);
                        target->port = atoi (port);
                        return target;
                    }
                }
            }
        }

        // cleanup
        if (tokens) {
            list_item_t *item = NULL;
            while ((item = I (List)->pop (tokens))) {
                if (item->data) free (item->data);
                I (ListItem)->destroy (&item);
            }
            I (List)->destroy (&tokens);
        }
    }
    return NULL;
}

static void
destroyHostPortPair (host_port_t **ptr) {
    if (ptr) {
        host_port_t *hp = *ptr;
        if (hp->host) free (hp->host);
        free (hp);
        *ptr = NULL;
    }
}

IMPLEMENT_INTERFACE (HostPort) = {
    .new     = newHostPortPair,
    .destroy = destroyHostPortPair
};
