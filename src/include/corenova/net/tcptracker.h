#ifndef __tcptracker_H__
#define __tcptracker_H__

#include <corenova/interface.h>
#include <corenova/data/message.h>
#include <corenova/net/protocol.h>
#include <corenova/net/transport.h>
#include <corenova/sys/quark.h>
#include <corenova/data/list.h>

#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <sys/types.h> 
#include <netinet/in_systm.h> 
#include <netinet/ip.h> 
#include <netinet/tcp.h> 
#include <netinet/udp.h> 
#include <net/if_arp.h> 
#include <netinet/if_ether.h> /* includes net/ethernet.h */ 

typedef struct {

 struct in_addr src_addr, dst_addr;
 uint16_t src_port, dst_port;
 
 uint32_t lastseen;

} tcpsession_entry_t;

typedef struct {

    list_t *session_table;
    MUTEX_TYPE lock;
    quark_t cleaner;
    uint32_t timeOut;

} tcpsession_table_t;

DEFINE_INTERFACE (TCPSessionTable) {

	tcpsession_table_t *(*new)     (uint32_t timeOut);
	tcpsession_entry_t *(*find)    (tcpsession_table_t *, in_addr_t *src_addr, in_addr_t *dst_addr, in_port_t src_port, in_port_t dst_port);
	tcpsession_entry_t *(*add)     (tcpsession_table_t *, in_addr_t *src_addr, in_addr_t *dst_addr, in_port_t src_port, in_port_t dst_port);
	void                (*remove)  (tcpsession_table_t *, in_addr_t *src_addr, in_addr_t *dst_addr, in_port_t src_port, in_port_t dst_port);
	uint32_t            (*count)   (tcpsession_table_t *);
	void                (*clear)   (tcpsession_table_t *);
	void                (*destroy) (tcpsession_table_t **);
	
};

#endif
