#ifndef __nfqueue_H__
#define __nfqueue_H__

#include <corenova/interface.h>

/*----------------------------------------------------------------*/


//#ifdef HAVE_LINUX_NETFILTER_H

//keen
//#ifdef HAVE_LIBNFQUEUE
#ifdef HAVE_LIBNFNETLINK

# include <netinet/in.h>
# include <linux/netfilter.h>
# include <libnetfilter_queue/libnetfilter_queue.h>

typedef struct {

    int id;
    int size;
    char *data;
    int verdict;
    
    struct nfq_q_handle *qh;

} nfqueue_packet_t;

DEFINE_INTERFACE (NetfilterPacket) {
    
	nfqueue_packet_t *(*new)        (int id, int size, char *payload, struct nfq_q_handle *);
    void              (*setVerdict) (nfqueue_packet_t *);
    void              (*destroy)    (nfqueue_packet_t **);
    
};

/*----------------------------------------------------------------*/

#include <corenova/data/queue.h>
#include <corenova/data/parameters.h>

#define NETFILTER_QUEUE_MAXSIZE 1024

typedef struct {

    cqueue_t *packetQueue;

    struct nfq_handle *handle;
    struct nfq_q_handle *queue;
    int fd;
    
} nfqueue_t;

DEFINE_INTERFACE (NetfilterQueue) {

    nfqueue_t        *(*new)        (parameters_t *);
    boolean_t         (*put)        (nfqueue_t *, nfqueue_packet_t *);
    nfqueue_packet_t *(*get)        (nfqueue_t *);
    void              (*destroy)    (nfqueue_t **);
    
};

#endif  /* HAVE_LINUX_NETFILTER_H */

#endif
