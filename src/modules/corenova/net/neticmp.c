#include <corenova/source-stub.h>

THIS = {
	.version     = "1.0",
	.author      = "Aleksanders Vinogradovs <alex.burkoff@gmail.com>",
	.description = "This module enables ICMP network operations.",
	.requires    = LIST (NULL),
	.implements  = LIST ("NetICMP"),
};

#include <corenova/net/neticmp.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <sys/socket.h>
#include <arpa/inet.h>			/* for inet_ntoa */
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <strings.h>

/*//////// API ROUTINES //////////////////////////////////////////*/

#define	DEFDATALEN	56
#define DEFTTL		30
#define	MAXIPLEN	(sizeof(struct ip) + MAX_IPOPTLEN)

static u_short in_cksum(u_short *, int);
static boolean_t _verify(neticmp_t *);
static int _ttime(neticmp_t *);

static unsigned short unique_ident = 0;

static neticmp_t*
_new (uint32_t timeout) {

    neticmp_t *p = calloc(1, sizeof(neticmp_t));
    
    p->sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);

    if(p->sock <= 0) {
    
        DEBUGP(DERR, "_new", "can't create raw socket");
        free(p);
        return NULL;
    
    }

    p->ident = unique_ident++;
    p->timeout = timeout;
    p->datalen = DEFDATALEN;
    p->ttl = DEFTTL;

    return p;

}

static boolean_t
_bind (neticmp_t *p, uint32_t bind_address) {

    boolean_t result = FALSE;

    if(p) {

        if(p->sock) {

            struct sockaddr_in ba;
            bzero(&ba, sizeof(ba));
            ba.sin_family = AF_INET;
            ba.sin_addr.s_addr = bind_address;

            if(bind(p->sock, (struct sockaddr *)&ba, sizeof(ba)) == 0) {

                result = TRUE;

            } else {

                DEBUGP(DERR, "_bind", "bind(): %s", strerror(errno));
                result = FALSE;

            }


        }


    }

    return result;

}

static void
_destroy (neticmp_t **pPtr) {

    if (pPtr) {
	
        neticmp_t *p = *pPtr;

	if (p) {
	
	    if(p->sock)
		close(p->sock);

	    free (p);
	    *pPtr = NULL;
		
	}
	
    }
    
}

static boolean_t
_pong(neticmp_t *p, uint32_t src_ip) {

    struct timeval now, tout, last;
    fd_set rfds;
    int n, cc;
    struct sockaddr_in from;

    if(!p || p->sock <= 0) {

        DEBUGP(DERR, "_pong", "invalid argument");
        return FALSE;

    }

    bzero(&from, sizeof(from));
    from.sin_family = AF_INET;
    //from.sin_len = sizeof(struct sockaddr_in);
    from.sin_addr.s_addr = src_ip;

    gettimeofday(&last, NULL);
    
    tout.tv_sec = (p->timeout*1000) / 1000000;
    tout.tv_usec = (p->timeout*1000) % 1000000;

    // tv_sec = 0, tv_usec = 1000
    
    FD_ZERO(&rfds);
    FD_SET(p->sock, &rfds);
    
    while(tout.tv_sec > 0 || tout.tv_usec > 0) {

        gettimeofday(&now, NULL);

        // tv_sec = 0, tv_usec = 1000
        // (100 usec passed)
        // tv_sec = 0, tv_usec = 900
        // (100 usec passed)
        // tv_sec = 0, tv_usec = 700
        tout.tv_sec -= (now.tv_sec - last.tv_sec);
        tout.tv_usec -= (now.tv_usec - last.tv_usec);

        last.tv_sec = now.tv_sec;
        last.tv_usec = now.tv_usec;

        while (tout.tv_usec < 0) {
            tout.tv_usec += 1000000;
            tout.tv_sec--;
        }
        
        while (tout.tv_usec >= 1000000) {
            tout.tv_usec -= 1000000;
            tout.tv_sec++;
        }
	
        if (tout.tv_sec < 0)
            tout.tv_sec = tout.tv_usec = 0;

//        DEBUGP (DINFO,"_pong","TIMEOUT tv_sec:%u tv_usec:%u",tout.tv_sec, tout.tv_usec);
        
        n = select(p->sock + 1, &rfds, NULL, NULL, &tout);
    
        if(n < 0)
            continue;
    
        if(n == 1) {
    
            unsigned int fromlen = sizeof(from);

    	    if ((cc = recvfrom(p->sock, p->ipacket, sizeof(p->ipacket), 0, (struct sockaddr*)&from, &fromlen)) < 0) {
                if (errno == EINTR && !SystemExit) {
                    continue;
                } else {
                    DEBUGP(DERR, "_ping", "recvmsg(): %s", strerror(errno));
                    return FALSE;
                }
            } else {
                if(_verify(p)) {
                    // pop out the packet
                    p->nrecv++;
                    gettimeofday(&p->last_in, NULL);
                    p->tottm+= _ttime(p);
                    return TRUE;
		    
                } else { // packet was received, but isn't ours
                    //usleep (1000);
                    continue;
                }
            }
        }
    
        if(n == 0) {
//	    DEBUGP(DINFO, "_pong", "timed out");
            return FALSE;
        }
    }
    
    return FALSE;

}

static int _ttime(neticmp_t *p) {

    if(!p)
	return 0;

    return ((p->last_in.tv_sec * 1000000 + p->last_in.tv_usec) - (p->last_out.tv_sec * 1000000 + p->last_out.tv_usec)) / 1000;

}

static boolean_t
_ping(neticmp_t *p, uint32_t dst_ip) {

    struct icmp *icp;
    int cc, i;

    struct sockaddr_in whereto;

    if(!p || p->sock <= 0) {

        DEBUGP(DERR, "_ping", "invalid argument");
        return FALSE;

    }

    bzero(&whereto, sizeof(whereto));
    
    whereto.sin_family = AF_INET;
    //whereto.sin_len = sizeof(struct sockaddr_in);
    whereto.sin_addr.s_addr = dst_ip;
    
    icp = (struct icmp *)p->opacket;
    icp->icmp_type = ICMP_ECHO;
    icp->icmp_code = 0;
    icp->icmp_cksum = 0;
    icp->icmp_seq = htons(p->nsent);
    icp->icmp_id = p->ident;

    gettimeofday(&p->last_out, NULL);

    cc = ICMP_MINLEN + p->datalen;

    icp->icmp_cksum = in_cksum((u_short *)icp, cc);

    i = sendto(p->sock, (char *)p->opacket, cc, 0, (struct sockaddr *)&whereto, sizeof(whereto));
    
    if (i < 0 || i != cc)  {
    
        DEBUGP(DERR, "_ping", "%s", strerror(errno));
        return FALSE;
    }
    
    p->nsent++;
    return TRUE;

}

static boolean_t _verify(neticmp_t *p) {

    struct ip *ip = (struct ip*)p->ipacket;
    int hlen = ip->ip_hl << 2;
    struct icmp *icmp = (struct icmp *)(p->ipacket+hlen);
    
//    DEBUGP (DINFO,"_verify","pid:%d type:%d id:%d seq:%d", p->ident, icmp->icmp_type, icmp->icmp_id, htons (icmp->icmp_seq) );
//    if(icmp->icmp_type == ICMP_ECHOREPLY && icmp->icmp_id == p->ident) {
    if(icmp->icmp_type == ICMP_ECHOREPLY && icmp->icmp_id == p->ident && htons(icmp->icmp_seq) == (p->nsent-1)) {
        return TRUE;
    }
    
    return FALSE;

}

u_short
in_cksum(u_short *addr, int len) {

    int nleft, sum;
    u_short *w;

    union {
	u_short	us;
	u_char	uc[2];
    } last;

    u_short answer;

    nleft = len;
    sum = 0;
    w = addr;

    while (nleft > 1)  {

	sum += *w++;
	nleft -= 2;
    
    }

    if (nleft == 1) {

	last.uc[0] = *(u_char *)w;
	last.uc[1] = 0;
	sum += last.us;

    }

    sum = (sum >> 16) + (sum & 0xffff);/* add hi 16 to low 16 */
    sum += (sum >> 16);/* add carry */
    answer = ~sum;/* truncate to 16 bits */

    return(answer);
    
}

IMPLEMENT_INTERFACE (NetICMP) = {
	.destroy = _destroy,
	.new     = _new,
	.ping    = _ping,
	.pong    = _pong,
	.ttime   = _ttime,
        .bind    = _bind
};

