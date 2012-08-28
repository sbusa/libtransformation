/* 
 * File:   pcap.h
 * Author: alexv
 *
 * Created on December 7, 2010, 9:28 PM
 */

#ifndef _corenova_net_pcap_h_
#define	_corenova_net_pcap_h_

#ifndef __USE_BSD
#define __USE_BSD
#endif

#define __FAVOR_BSD

#include <pcap.h> /* if this gives you an error try pcap/pcap.h */
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//#include <sys/types.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>
#include <netinet/if_ether.h> /* includes net/ethernet.h */
#include <unistd.h>

#if defined(freebsd8) || defined(freebsd7)
#include <net/if.h>
#endif
#if defined(linux)
#include <linux/netdevice.h>
#endif

#include <corenova/sys/quark.h>
#include <corenova/core.h>

enum {

    CALLBACK_ACCOUNTING,
    CALLBACK_RAW,
    CALLBACK_IPACCOUNTING,
    CALLBACK_PCAP_RAW

};

typedef struct {

    unsigned short proto_ip;
    struct in_addr src;
    struct in_addr dst;
    unsigned short sport;
    unsigned short dport;
    unsigned int len;

} pcap_accounting_t;

typedef struct {

    int len;
    int link_type;        
    char *data;

} pcap_raw_t;

typedef struct {
    pcap_t *descr;
    char *ifname;
    quark_t *quark;
    boolean_t active;
    void (*callback)(char *, void *data);
    int callback_type;
    char errbuf[PCAP_ERRBUF_SIZE];
    int link_type;

} pcap_instance_t;

DEFINE_INTERFACE(Pcap) {
    pcap_instance_t * (*new) (char *ifname, void (*callback)(char *ifname, void *data), int callback_type);
    boolean_t(*start) (pcap_instance_t *);
    boolean_t(*stop) (pcap_instance_t *);
    void (*filter) (pcap_instance_t *, char *);
    void (*destroy) (pcap_instance_t **);
};


#endif

