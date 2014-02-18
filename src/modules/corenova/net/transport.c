#include <corenova/source-stub.h>

THIS = {
	.version     = "2.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "This module creates an abstraction to underlying network transport.",
	.implements  = LIST ("Transport"),
	.requires    = LIST ("corenova.net.ssl",
                         "corenova.net.tcp")
};

#include <corenova/net/transport.h>
#include <errno.h>
#include <poll.h>

/* Transport routines */

static transport_t *
_new (enum transport_type type, void *connection) {
    if (type && connection) {
        transport_t *transport = (transport_t *)calloc (1,sizeof (transport_t));
        if (transport) {
            transport_info_t *info = &transport->info;
            transport->type = type;
            transport->connection.ssl = NULL;
            if (connection) {
                switch (type) {
                  case TRANSPORT_SSL: {
                      ssl_t *ssl = (ssl_t *)connection;
                      tcp_t *tcp = ssl->tcp;
                      info->socket = tcp->sock;
                      info->shost  = tcp->srcHostName;
                      info->saddr  = tcp->srcHostAddr.sin_addr.s_addr;
                      info->sport  = tcp->srcHostPort;
                      info->dhost  = tcp->destHostName;
                      info->daddr  = tcp->destHostAddr.sin_addr.s_addr;
                      info->dport  = tcp->destHostPort;
                      transport->connection.ssl = ssl;
                      break;
                  }
                  case TRANSPORT_TCP: {
                      tcp_t *tcp = (tcp_t *)connection;
                      info->socket = tcp->sock;
                      info->shost  = tcp->srcHostName;
                      info->saddr  = tcp->srcHostAddr.sin_addr.s_addr;
                      info->sport  = tcp->srcHostPort;
                      info->dhost  = tcp->destHostName;
                      info->daddr  = tcp->destHostAddr.sin_addr.s_addr;
                      info->dport  = tcp->destHostPort;
                      transport->connection.tcp = tcp;
                      break;
                  }
                  case TRANSPORT_UDP: {
                      udp_t *udp = (udp_t *)connection;
                      info->socket = udp->sock;
                      info->shost  = udp->srcHostName;
                      info->saddr  = udp->srcHostAddr.sin_addr.s_addr;
                      info->sport  = udp->srcHostPort;
                      info->dhost  = udp->destHostName;
                      info->daddr  = udp->destHostAddr.sin_addr.s_addr;
                      info->dport  = udp->destHostPort;
                      transport->connection.udp = udp;
                      break;
                  }
                  default:
                      DEBUGP (DERR,"new","unsupported transport mechanism: %d",type);
                      I (Transport)->destroy (&transport);
                }
            } else {
                DEBUGP (DERR,"new","unable to create without valid connection!");
                I (Transport)->destroy (&transport);
            }
        }
        return transport;
    }
    return NULL;
}

static void
_destroy (transport_t **transportPtr) {

	if (transportPtr) {

		DEBUGP(DDEBUG, "_destroy", "destroying transport %p", *transportPtr);

		transport_t * transport = *transportPtr;
		if (transport) {
			switch (transport->type) {
                case TRANSPORT_SSL: {
                     ssl_t *ssl = transport->connection.ssl;
                     if (ssl) {
                         tcp_t *tcp = ssl->tcp;
                         I (SSLConnector)->destroy (&transport->connection.ssl);
                         /* Ravi: Found in my testing that tcp destroy need not be called when doing SSL Shutdown.
                          * It fixes un-needed RST issue in proxy.
                          */
                         I (TcpConnector)->destroy (&tcp); 
                     }
                     break;
                }
		case TRANSPORT_TCP:
			I (TcpConnector)->destroy (&transport->connection.tcp);
		        break;
		case TRANSPORT_UDP:
			I (UdpConnector)->destroy (&transport->connection.udp);
			break;
		}
		free (transport);
                *transportPtr = NULL;
		}
	}
	
}

static transport_info_t *
getTransportInfo (transport_t *transport) {
    if (transport) {
        return &transport->info;
    }
    return NULL;
}

static int
_poll (transport_t *transport, transport_poll_type_t type, int timeout) {
    if (transport) {
        struct timeval tv1, tv2;
        struct pollfd fds;
        if (transport->info.socket->skd) {
            fds.fd = transport->info.socket->skd;
        
            switch (type) {
              case TRANSPORT_POLLIN:    fds.events = POLLIN; break;
              case TRANSPORT_POLLOUT:   fds.events = POLLOUT; break;
              case TRANSPORT_POLLINOUT: fds.events = POLLIN | POLLOUT; break;
            }
        
            while (TRUE) {
                gettimeofday (&tv1,NULL);
                switch (poll (&fds, 1, timeout)) {
                  case -1:
                      if (errno == EINTR && !SystemExit) {
                          if (timeout > 0) {
                              gettimeofday (&tv2,NULL);
                              timeout -= ((tv2.tv_sec - tv1.tv_sec) * 1000) + ((tv2.tv_usec - tv1.tv_usec) / 1000);
                              if (timeout > 0) continue;
                          } else continue;   
                      }
                  case 0:
                      return TRANSPORT_TIMEOUT;
                }
                if (fds.revents & POLLERR || fds.revents & POLLHUP || fds.revents & POLLNVAL) {
                    DEBUGP (DWARN,"_poll","transport socket has issues!");
                    return TRANSPORT_FATAL;
                }
                return TRANSPORT_CONTINUE;
            }
        }
    }
    return TRANSPORT_FATAL;
}

static uint32_t
_send (transport_t *transport, char *buf, uint32_t size) {
	if (transport) {
		switch (transport->type) {
		case TRANSPORT_SSL:
			return I (SSLConnector)->write (transport->connection.ssl,buf,size);
		case TRANSPORT_TCP:
			return I (TcpConnector)->write (transport->connection.tcp,buf,size);
		case TRANSPORT_UDP:
			return I (UdpConnector)->write (transport->connection.udp,buf,size);
		}
	}
	DEBUGP (DERR,"send","unable to send any data over the transport!");
	return 0;
}

static uint32_t
_recv (transport_t *transport, char **buf, uint32_t size) {
	if (transport) {
		switch (transport->type) {
		case TRANSPORT_SSL:
			return I (SSLConnector)->read (transport->connection.ssl,buf,size);
		case TRANSPORT_TCP:
			return I (TcpConnector)->read (transport->connection.tcp,buf,size);
		case TRANSPORT_UDP:
			return I (UdpConnector)->read (transport->connection.udp,buf,size);
		}
	}
	DEBUGP (DERR,"recv","unable to receive any data over the transport!");
	return 0;
}

/*
 * XXX - this is a MAJOR HACK!!!!
 *
 * A better way may be to support this inside SSL module or such...
 */ 
static void 
_forcerawtcp(transport_t *transport) {
	if(transport && transport->type == TRANSPORT_SSL) {
        //ssl_t *ssl = transport->connection.ssl;
        DEBUGP (DINFO,"forcerawtcp","falling back to standard TCP communication!");
        transport->connection.tcp = transport->connection.ssl->tcp;
	    transport->type = TRANSPORT_TCP;
        /*
         * XXX - no need to free if part of transformation... but need to revisit this logic.
         */
//        I (SSLConnector)->free (&ssl);
	}
}

static void
_setTimeout(transport_t *transport, int secs) {
	if (transport) {
		switch (transport->type) {
		    case TRANSPORT_SSL:
			    I (SSLConnector)->setTimeout(transport->connection.ssl,secs);
			    break;
		    case TRANSPORT_TCP:
			    I (TcpConnector)->setTimeout(transport->connection.tcp,secs);
			    break;
		    case TRANSPORT_UDP:
			    /* not supported yet... */
			    break;
		}
	}
}

static void
_useRecords (transport_t *transport, boolean_t state) {
	if (transport) {
		switch (transport->type) {
		    case TRANSPORT_SSL:
			    I (SSLConnector)->useRecords(transport->connection.ssl,state);
			    break;
		    case TRANSPORT_TCP:
			    I (TcpConnector)->useRecords(transport->connection.tcp,state);
			    break;
		    case TRANSPORT_UDP:
			    /* not supported yet... */
			    break;
		}
	}
}

IMPLEMENT_INTERFACE (Transport) = {
	.new         = _new,
	.destroy     = _destroy,
    .info        = getTransportInfo,
    .poll        = _poll,
	.send        = _send,
	.recv        = _recv,
	.forcerawtcp = _forcerawtcp,
	.setTimeout  = _setTimeout,
    .useRecords  = _useRecords
};

