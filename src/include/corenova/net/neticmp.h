#ifndef __netimcp_H__
#define __neticmp_H__

#include <corenova/interface.h>

#include <sys/param.h>
#include <sys/socket.h>
//#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
                                                                                                                                                                                                               
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <math.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

typedef struct {

	int sock;
/*	u_char opacket[IP_MAXPACKET], ipacket[IP_MAXPACKET]; */
/*	package aligned 4 byte to Compatible EABI;*/
        u_char opacket[IP_MAXPACKET] __attribute__((aligned(4)));
	u_char ipacket[IP_MAXPACKET] __attribute__((aligned(4)));
	
	int nsent;
	int nrecv;
	int avgtm;
	int tottm;
	u_short ident;
	int options;
	int timeout;
	
	int datalen;
	int ttl;
	
	struct timeval last_out;
	struct timeval last_in;

} neticmp_t;

DEFINE_INTERFACE (NetICMP) {

	void       (*destroy)    (neticmp_t **);
	neticmp_t *(*new)        (uint32_t timeout);
        boolean_t  (*bind)       (neticmp_t *, uint32_t);
	boolean_t  (*ping)       (neticmp_t *, uint32_t);
	boolean_t  (*pong)       (neticmp_t *, uint32_t);
	int        (*ttime)      (neticmp_t *);


};

#endif
