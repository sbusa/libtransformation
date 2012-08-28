#include <corenova/source-stub.h>

THIS = {
	.version     = "2.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "This module enables UDP-oriented network operations.",
	.requires    = LIST ("corenova.net.socket", "corenova.net.resolve"),
	.implements  = LIST ("UdpConnector"),
};

#include <corenova/net/udp.h>
#include <corenova/net/resolve.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <sys/socket.h>
#include <arpa/inet.h>			/* for inet_ntoa */
#include <errno.h>
#include <unistd.h>
#include <netdb.h>

/*//////// API ROUTINES //////////////////////////////////////////*/

static udp_t *
_connect (const char *host, u_int16_t port) {

    udp_t *udp = (udp_t *)calloc(1,sizeof(udp_t));

    if (udp) {

	udp->sock = I (Socket)->new (SOCKET_DGRAM);

	if (udp->sock) {

	    struct hostent *hp = gethostbyname (host);

	    if (hp) {

		udp->destHostAddr.sin_addr =* (struct in_addr *) (hp->h_addr_list[0]);
		udp->destHostAddr.sin_family = AF_INET;
		udp->destHostAddr.sin_port = htons (port);
		
		if(connect(udp->sock->skd, (struct sockaddr *)&udp->destHostAddr,sizeof(struct sockaddr_in)) != 0) {
		
		    perror("connect");
		
		    DEBUGP(DERR, "connect", "unable to set remote address");		
		    I (UdpConnector)->destroy (&udp);
		
		}

	    } else {
	
		DEBUGP(DERR,"connect","unable to resolve host (%s)",host);
		I (UdpConnector)->destroy (&udp);
	    
	    }
	    
	} else {
	
	    DEBUGP(DERR,"connect","unable to create network socket");
	    I (UdpConnector)->destroy (&udp);
	
	}

    }
    
    return (udp);

}

static udp_t *
_bconnect (const char *host, u_int16_t port) {

    udp_t *udp = (udp_t *)calloc(1,sizeof(udp_t));

    if (udp) {

	udp->sock = I (Socket)->new (SOCKET_DGRAM);
	I (Socket)->setOpt(udp->sock, BCAST);

	if (udp->sock) {

	    struct hostent *hp = gethostbyname (host);

	    if (hp) {

		udp->destHostAddr.sin_addr =* (struct in_addr *) (hp->h_addr_list[0]);
		udp->destHostAddr.sin_family = AF_INET;
		udp->destHostAddr.sin_port = htons (port);
		
		if(connect(udp->sock->skd, (struct sockaddr *)&udp->destHostAddr,sizeof(struct sockaddr_in)) != 0) {
		
		    perror("bconnect");
		
		    DEBUGP(DERR, "bconnect", "unable to set remote address");		
		    I (UdpConnector)->destroy (&udp);
		
		}

	    } else {
	
		DEBUGP(DERR,"bconnect","unable to resolve host (%s)",host);
		I (UdpConnector)->destroy (&udp);
	    
	    }
	    
	} else {
	
	    DEBUGP(DERR,"bconnect","unable to create network socket");
	    I (UdpConnector)->destroy (&udp);
	
	}

    }
    
    return (udp);

}

static void
_destroy (udp_t **udpPtr) {

    if (udpPtr) {

	udp_t *udp = *udpPtr;
	if (udp) {

	    I (Socket)->destroy (&udp->sock);
	    free (udp);
	    *udpPtr = NULL;
	
	}
	
    }
}
	
static socket_t *
_listen (u_int16_t port) {

    socket_t *sock = I (Socket)->new (SOCKET_DGRAM);
    if (sock) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(struct sockaddr_in));
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
			
        if (bind(sock->skd,(struct sockaddr *)&addr,sizeof(struct sockaddr_in))>=0){

            // all cool

        } else {
            DEBUGP(DERR,"listen","unable to bind to port %d",port);
            I (Socket)->destroy (&sock);
        }
    }
    return sock;
}

static socket_t *
_listen2 (in_addr_t host, u_int16_t port) {

    socket_t *sock = I (Socket)->new (SOCKET_DGRAM);
    if (sock) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(struct sockaddr_in));
        addr.sin_addr.s_addr = host;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
			
        if (bind(sock->skd,(struct sockaddr *)&addr,sizeof(struct sockaddr_in))>=0){

            // all cool

        } else {
            DEBUGP(DERR,"listen","unable to bind to host %s, port %d",I (Resolve)->ip2string(host), port);
            I (Socket)->destroy (&sock);
        }
    }
    return sock;
}

static uint32_t
_udpRead (udp_t *udp, char **buf, uint32_t size) {

    boolean_t dynamicBuffer = FALSE;
    int32_t returnValue = 0, totalReceived = 0, recordSize = 0;
    uint32_t delay = 10000;
    
    if(udp && buf && size) {
    
	if (!*buf) {
    
	    dynamicBuffer = TRUE;
	    
	    *buf = (char *) calloc(size+1, sizeof(char));
	    (*buf)[size] = '\0';
	    
	    if(*buf) {
	    
		 
	    } else {
	    
		DEBUGP (DERR,"_udpRead","cannot allocate memory for %lu bytes!",(unsigned long)size+1);
		return 0;
	    
	    }
	
	}
	
	while(totalReceived < sizeof(recordSize)) {
	
	    if((returnValue = recv(udp->sock->skd, ((char*)&recordSize)+totalReceived, sizeof(recordSize)-totalReceived, 0)) < 0) {
	
		if(errno == EINTR && !SystemExit)
		    continue;
	
		if(dynamicBuffer)
		    free(*buf);
			
		DEBUGP(DERR, "_udpRead", "unable to receive record size");
		return 0;
		
	    } else
	    
	    if(returnValue == 0) {
	    
		DEBUGP(DALL, "_udpRead", "peer closed the connection");
		return 0;
	    
	    }
	    
	    totalReceived += returnValue;

	}
	
	if(recordSize > size) {

	    if(dynamicBuffer)
		free(*buf);
	
	    DEBUGP(DERR, "_udpRead", "record(%d) is too large for allocated buffer (%d)", recordSize, size);
	    return 0;
	
	}
	
	totalReceived = 0;
	
	while(totalReceived < size) {
	    
	    if((returnValue = recv(udp->sock->skd, *buf+totalReceived, size-totalReceived, 0)) < 0) {
	    
		if(errno == EINTR && !SystemExit) {
		
		    DEBUGP(DDEBUG, "_udpRead", "signal interrupted recv() call (%s), retrying", strerror(errno));
		    continue;
		
		}
	    
		DEBUGP (DERR,"_udpRead","udp socket read error (%s)", strerror(errno));
		if(dynamicBuffer)
		    free(*buf);
		    
		totalReceived = 0;
		break;
	    
	    } else
	    
	    if(returnValue == 0) {
	    
		DEBUGP(DALL, "_udpRead", "peer closed the connection");
		break;		
	    
	    }
	    
	    totalReceived += returnValue;
	    
	    if(totalReceived < size) {
	
		DEBUGP(DALL, "_udpRead", "interrupted recv() (%s), retrying", strerror(errno));
		usleep (delay);
		delay *= 2;
	    
	    }
		
	}
		
    }
    
    return totalReceived;

}

static uint32_t
_udpWrite (udp_t *udp, char *buf, uint32_t size) {

    int32_t returnValue = 0, totalSent = 0;
    uint32_t delay = 10000;
    
    if(udp && udp->sock && buf && size) {
    
	while(totalSent < sizeof(size)) {
    
	    if((returnValue = send(udp->sock->skd, ((char*)&size)+totalSent, sizeof(size)-totalSent, 0)) < 0) {
	    
		if(errno == EINTR && !SystemExit)
		    continue;
	    
		DEBUGP (DERR, "_udpWrite", "error while sending record size");
		return 0;
	
	    }
	    
	    totalSent += returnValue;
	
	}
	
	totalSent = 0;
	
	while(totalSent < size) {
    
	    if((returnValue = send(udp->sock->skd, buf+totalSent, size-totalSent, 0)) < 0) {
	
		DEBUGP (DERR, "_udpWrite", "udp socket write error (errno %d)", errno);
		totalSent = 0;
		break;
	
	    } else
	    
	    if(returnValue == 0) {
	    
		DEBUGP (DALL, "_udpWrite", "peer closed the connection");
		break;
	    
	    }
	    
	    totalSent += returnValue;
	    
	    if(totalSent < size) {

		DEBUGP (DERR, "_udpWrite", "interrupted send() (errno %d), retrying", errno);
		usleep (delay);
		delay *= 2;
		
	    }
		
	}
    
    }

    return totalSent;

}

IMPLEMENT_INTERFACE (UdpConnector) = {
    .destroy = _destroy,
    .connect = _connect,
    .bconnect = _bconnect,
    .listen  = _listen,
    .listen2 = _listen2,
    .read    = _udpRead,
    .write   = _udpWrite,
};

