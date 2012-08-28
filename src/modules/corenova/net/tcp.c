#include <corenova/source-stub.h>

THIS = {
	.version     = "2.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "This module enables TCP-oriented network operations.",
	.requires    = LIST ("corenova.net.socket",
                         "corenova.net.resolve",
                         "corenova.sys.signals",
                         "corenova.data.string",
                         "corenova.net.route"),
	.implements  = LIST ("TcpConnector")
};

#include <corenova/net/tcp.h>
#include <corenova/net/socket.h>
#include <corenova/net/resolve.h>
#include <corenova/data/string.h>
#include <corenova/sys/signals.h>
#include <corenova/net/route.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

/*//////// API ROUTINES //////////////////////////////////////////*/

static tcp_t *
_connect2 (const char *host, u_int16_t port, const char *ifname) {
	tcp_t *tcp = (tcp_t *)calloc(1,sizeof(tcp_t));
	if (tcp) {
        /*
         * below is a HACK for now to preserve previous interface API
         */
        DEBUGP (DDEBUG,"connect2","resolving %s",host);
        in_addr_t ip = I (Resolve)->name2ip (host);
        if (ip != INADDR_NONE) {
            tcp->sock = I (Socket)->new (SOCKET_STREAM);

            if(tcp->sock && ifname) {

                struct sockaddr_in addr = { .sin_addr.s_addr = I (Route)->getIfIP((char *)ifname), .sin_family = AF_INET };
                
#ifdef freebsd8                
                addr.sin_len = sizeof(struct sockaddr_in);
#endif

                if(!I (Socket)->bind(tcp->sock, (struct sockaddr*)&addr)) {

                    DEBUGP(DERROR, "_connect2", "failed to bind to '%s'", ifname);

                }

            }

        } else {
            tcp->sock = I (Socket)->new (SOCKET_UNIX);
        }
        tcp->records = FALSE;
        if (tcp->sock) {
            char *ipstring = I (Resolve)->ip2string (ip); /* optimization hack */
            DEBUGP (DDEBUG,"connect2","setting up socket for %s:%d",ipstring,port);
            if (I (Socket)->setAddress (tcp->sock, ipstring, port)) {
                tcp->destHostName = I (String)->copy (host);
                tcp->destHostPort = port;
                free (ipstring); /* done using this hack */
                /*
                 * another HACK
                 */
                if (tcp->sock->type == SOCKET_STREAM) {
                    memcpy (&tcp->destHostAddr, &tcp->sock->addr, tcp->sock->len);
                }

                /*
                 * here, we use nonblocking connect method!
                 */
                int flags = fcntl (tcp->sock->skd, F_GETFL, 0);
                if (fcntl (tcp->sock->skd, F_SETFL, flags | O_NONBLOCK) < 0) {
                    DEBUGP (DERR,"connect2","unable to set socket into NONBLOCKING mode!");
                    I (TcpConnector)->destroy (&tcp);
                    return NULL;
                }

			  try_connect:
				DEBUGP(DINFO,"connect2","making a connection to %s:%d (%u attempt)",host,port,tcp->retryCounter+1);
				if (connect(tcp->sock->skd,(struct sockaddr *)&tcp->sock->addr,tcp->sock->len)==0) {

                    if (fcntl (tcp->sock->skd, F_SETFL, flags) == 0) {
                        tcp->retryCounter = 0; /* reset to 0 */
                        DEBUGP (DINFO,"connect2","connection to %s:%d successful!",host,port);
                        return tcp;
                    }
                    DEBUGP (DERR,"connect2","unable to set socket back into BLOCKING mode!");
				} else {
					switch(errno) {
                      case EISCONN:
                          DEBUGP(DERR,"connect2","already connected?!? tear-down & reopen socket!");
                          I (Socket)->destroy (&tcp->sock);
                          tcp->sock = I (Socket)->new (SOCKET_STREAM);
                          if (!tcp->sock) {
                              DEBUGP (DERR,"connect2","unable to create a new socket!");
                              break;
                          }
                      case EAGAIN:
                      case EINTR:
                          if(SystemExit)
                              break;
                          goto try_connect;

                      case EINPROGRESS:
                      case EALREADY: {
                          struct timeval tv = {
                              .tv_sec = TCP_CONNECT_TIMEOUT,
                              .tv_usec = 0
                          };
                          fd_set myset;
                          FD_ZERO (&myset);
                          FD_SET (tcp->sock->skd,&myset);
                          if (select (tcp->sock->skd+1, NULL, &myset, NULL, &tv) > 0) {
                              socklen_t lon = sizeof (int);
                              int valopt;
                              if (getsockopt (tcp->sock->skd, SOL_SOCKET, SO_ERROR, (void*) (&valopt), &lon) == 0) {
                                  if (!valopt) {

                                      if (fcntl (tcp->sock->skd, F_SETFL, flags) == 0) {
                                          tcp->retryCounter = 0; /* reset to 0 */
                                          DEBUGP (DINFO,"connect2","connection to %s:%d successful! (via fcntl)",host,port);
                                          return tcp;
                                      }
                                      DEBUGP (DERR,"connect2","unable to set socket back into BLOCKING mode!");
                                  } else {
                                      DEBUGP (DERR,"connect2","error() %d - %s", valopt, strerror (valopt));
                                  }
                              } else {
                                  DEBUGP (DERR,"connect2","error in getsockopt() %d - %s", errno, strerror (errno));
                              }
                          }
                      }
                      case ETIMEDOUT:
                          if(SystemExit)
                              break;

                          if (++tcp->retryCounter < TCP_CONNECT_MAX_RETRY) {
                              if (tcp->sock->type == SOCKET_STREAM) {
                                  DEBUGP (DINFO,"connect2","re-resolving host (%s) for another connection attempt",host);
                                  in_addr_t ip2 = I (Resolve)->name2ip (host);
                                  if (ip2 != INADDR_NONE) {
                                      if (ip2 != ip) {
                                          ip = ip2;
                                          tcp->sock->addr.in.sin_addr.s_addr = ip;
                                          goto try_connect;
                                      } else {
                                          DEBUGP (DERR,"connect2","unable to re-resolve host (%s) to a different IP to retry",host);
                                      }
                                  } else {
                                      DEBUGP (DERR,"connect2","unable to re-resolve host (%s)",host);
                                  }
                              } else {
                                  goto try_connect;
                              }
                          } else {
                              DEBUGP (DERR,"connect2","too many retries! giving up!");
                          }
                          break;

                      default:
                          DEBUGP (DERR,"connect2","unhandled error (%s)",strerror (errno));
					}
                    DEBUGP (DERR,"connect2","unable to make a connection to (%s)",host);
					I (TcpConnector)->destroy (&tcp);
				}
			} else {
                free (ipstring); /* done using this hack */
				I (TcpConnector)->destroy (&tcp);
			}
		} else {
			DEBUGP(DERR,"connect2","unable to create network socket");
			I (TcpConnector)->destroy (&tcp);
		}
	}
	return (tcp);
}

static tcp_t *
_connect (const char *host, u_int16_t port) {
	tcp_t *tcp = (tcp_t *)calloc(1,sizeof(tcp_t));
	if (tcp) {
        /*
         * below is a HACK for now to preserve previous interface API
         */
        DEBUGP (DDEBUG,"connect","resolving %s",host);
        in_addr_t ip = I (Resolve)->name2ip (host);
        if (ip != INADDR_NONE) {
            tcp->sock = I (Socket)->new (SOCKET_STREAM);
        } else {
            tcp->sock = I (Socket)->new (SOCKET_UNIX);
        }
        tcp->records = FALSE;
        if (tcp->sock) {
            char *ipstring = I (Resolve)->ip2string (ip); /* optimization hack */
            DEBUGP (DDEBUG,"connect","setting up socket for %s:%d",ipstring,port);
            if (I (Socket)->setAddress (tcp->sock, ipstring, port)) {
                tcp->destHostName = I (String)->copy (host);
                tcp->destHostPort = port;
                free (ipstring); /* done using this hack */
                /*
                 * another HACK
                 */
                if (tcp->sock->type == SOCKET_STREAM) {
                    memcpy (&tcp->destHostAddr, &tcp->sock->addr, tcp->sock->len);
                }
                
                /*
                 * here, we use nonblocking connect method!
                 */
                int flags = fcntl (tcp->sock->skd, F_GETFL, 0);
                if (fcntl (tcp->sock->skd, F_SETFL, flags | O_NONBLOCK) < 0) {
                    DEBUGP (DERR,"connect","unable to set socket into NONBLOCKING mode!");
                    I (TcpConnector)->destroy (&tcp);
                    return NULL;
                }
                
			  try_connect:
				DEBUGP(DINFO,"connect","making a connection to %s:%d (%u attempt)",host,port,tcp->retryCounter+1);
				if (connect(tcp->sock->skd,(struct sockaddr *)&tcp->sock->addr,tcp->sock->len)==0) {

                    if (fcntl (tcp->sock->skd, F_SETFL, flags) == 0) {
                        tcp->retryCounter = 0; /* reset to 0 */
                        DEBUGP (DINFO,"connect","connection to %s:%d successful!",host,port);
                        return tcp;
                    }
                    DEBUGP (DERR,"connect","unable to set socket back into BLOCKING mode!");
				} else {
					switch(errno) {
                      case EISCONN:
                          DEBUGP(DERR,"connect","already connected?!? tear-down & reopen socket!");
                          I (Socket)->destroy (&tcp->sock);
                          tcp->sock = I (Socket)->new (SOCKET_STREAM);
                          if (!tcp->sock) {
                              DEBUGP (DERR,"connect","unable to create a new socket!");
                              break;
                          }
                      case EAGAIN:
                      case EINTR:
                          if(SystemExit)
                              break;
                          goto try_connect;

                      case EINPROGRESS:
                      case EALREADY: {
                          struct timeval tv = {
                              .tv_sec = TCP_CONNECT_TIMEOUT,
                              .tv_usec = 0
                          };
                          fd_set myset;
                          FD_ZERO (&myset);
                          FD_SET (tcp->sock->skd,&myset);
                          if (select (tcp->sock->skd+1, NULL, &myset, NULL, &tv) > 0) {
                              socklen_t lon = sizeof (int);
                              int valopt;
                              if (getsockopt (tcp->sock->skd, SOL_SOCKET, SO_ERROR, (void*) (&valopt), &lon) == 0) {
                                  if (!valopt) {

                                      if (fcntl (tcp->sock->skd, F_SETFL, flags) == 0) {
                                          tcp->retryCounter = 0; /* reset to 0 */
                                          DEBUGP (DINFO,"connect","connection to %s:%d successful! (via fcntl)",host,port);
                                          return tcp;
                                      }
                                      DEBUGP (DERR,"connect","unable to set socket back into BLOCKING mode!");
                                  } else {
                                      DEBUGP (DERR,"connect","error() %d - %s", valopt, strerror (valopt));
                                  }
                              } else {
                                  DEBUGP (DERR,"connect","error in getsockopt() %d - %s", errno, strerror (errno));
                              }
                          }
                      }
                      case ETIMEDOUT:
                          if(SystemExit)
                              break;
                          
                          if (++tcp->retryCounter < TCP_CONNECT_MAX_RETRY) {
                              if (tcp->sock->type == SOCKET_STREAM) {
                                  DEBUGP (DINFO,"connect","re-resolving host (%s) for another connection attempt",host);
                                  in_addr_t ip2 = I (Resolve)->name2ip (host);
                                  if (ip2 != INADDR_NONE) {
                                      if (ip2 != ip) {
                                          ip = ip2;
                                          tcp->sock->addr.in.sin_addr.s_addr = ip;
                                          goto try_connect;
                                      } else {
                                          DEBUGP (DERR,"connect","unable to re-resolve host (%s) to a different IP to retry",host);
                                      }
                                  } else {
                                      DEBUGP (DERR,"connect","unable to re-resolve host (%s)",host);
                                  }
                              } else {
                                  goto try_connect;
                              }
                          } else {
                              DEBUGP (DERR,"connect","too many retries! giving up!");
                          }
                          break;
                          
                      default:
                          DEBUGP (DERR,"connect","unhandled error (%s)",strerror (errno));
					}
                    DEBUGP (DERR,"connect","unable to make a connection to (%s)",host);
					I (TcpConnector)->destroy (&tcp);
				}
			} else {
                free (ipstring); /* done using this hack */
				I (TcpConnector)->destroy (&tcp);
			}
		} else {
			DEBUGP(DERR,"connect","unable to create network socket");
			I (TcpConnector)->destroy (&tcp);
		}
	}
	return (tcp);
}

static void
_destroy (tcp_t **tcpPtr) {
	if (tcpPtr) {
		tcp_t *tcp = *tcpPtr;
		if (tcp) {
			I (Socket)->destroy (&tcp->sock);
            if (tcp->srcHostName)  free (tcp->srcHostName);
            if (tcp->destHostName) free (tcp->destHostName);
			free (tcp);
			*tcpPtr = NULL;
		}
	}
}
	
static tcp_t *
_accept (socket_t *sock) {
	tcp_t *tcp = (tcp_t *)calloc(1,sizeof(tcp_t));
	if (tcp) {
		if (sock && sock->flags & SOCKET_LISTEN_FLAG) {
			tcp->sock = I (Socket)->new (SOCKET_DUMMY);
			if (tcp->sock) {
				uint32_t addrLen = sizeof (struct sockaddr_in);
			  try_accept:
				DEBUGP(DINFO,"accept","waiting for incoming connection...");
				if ((tcp->sock->skd = accept (sock->skd,(struct sockaddr *)&tcp->srcHostAddr, &addrLen))>=0) {
                    char *host = I (Resolve)->ip2string (tcp->srcHostAddr.sin_addr.s_addr);
					tcp->sock->flags |= SOCKET_ACCEPT_FLAG;
					tcp->srcHostPort = ntohs (tcp->srcHostAddr.sin_port);
					tcp->retryCounter = 0; /* reset to 0 */
					DEBUGP (DINFO,"accept","accepted incoming connection from %s:%u", host, tcp->srcHostPort);
                    tcp->srcHostName = host;
                    tcp->records = FALSE;
				} else {
					switch (errno) {
                      case EINTR:
                      case EAGAIN:
                          if(SystemExit)
                              break;
					
                          if (tcp->retryCounter++ < TCP_ACCEPT_MAX_RETRY) {
                              DEBUGP (DINFO,"accept","trying again (%u retries)...",tcp->retryCounter);
                              goto try_accept;
                          } else {
                              DEBUGP (DERR,"accept","too many retries! giving up!");
                          }
                          break;
                          
                      default:
                          DEBUGP (DERR,"accept","unhandled error: %s",strerror (errno));
					}
					I (TcpConnector)->destroy (&tcp);
				}
			} else {
				DEBUGP (DERR,"accept","unable to create network socket");
                I (TcpConnector)->destroy (&tcp);
			}
		} else {
			DEBUGP (DERR,"accept","invalid socket to accept from!");
            I (TcpConnector)->destroy (&tcp);
		}
	}
	return tcp;
}

static socket_t *
_listen (u_int16_t port) {
	socket_t *sock = I (Socket)->new (SOCKET_STREAM);
	if (sock) {
		struct sockaddr_in addr;
		int32_t val = 10; /* option for reusing addr */
		memset(&addr, 0, sizeof(struct sockaddr_in));
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		setsockopt(sock->skd,SOL_SOCKET,SO_REUSEADDR,&val,sizeof(val));
			
		if (bind(sock->skd,(struct sockaddr *)&addr,sizeof(struct sockaddr_in))>=0){
			if (listen(sock->skd, SOMAXCONN) >= 0) {
				sock->flags |= SOCKET_LISTEN_FLAG;
			} else {
				DEBUGP(DERR,"listen","%s",strerror(errno));
				I (Socket)->destroy (&sock);
			}
		} else {
			DEBUGP(DERR,"listen","unable to bind to port %d",port);
			I (Socket)->destroy (&sock);
		}
	}
	return sock;
}

/* not sure if this belongs here... */
static boolean_t
_setTimeout (tcp_t *tcp, int32_t seconds) {
	if (tcp && seconds >= 0) {
		struct timeval tv;
		tv.tv_sec = seconds;
		tv.tv_usec = 0;
		if (tcp->sock) {
			setsockopt(tcp->sock->skd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
			setsockopt(tcp->sock->skd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
			return TRUE;
		}
	}
	return FALSE;
	
}

static void
useRecords (tcp_t *tcp, boolean_t state) {
    if (tcp) {
        tcp->records = state;
    }
}

/*
  Implement sslRead-similar interface, but data is read
  into the buffer directly with a single system call.
*/
static uint32_t
_tcpRead (tcp_t *tcp, char **buf, uint32_t size) {

    boolean_t dynamicBuffer = FALSE;
    int32_t returnValue = 0, totalReceived = 0, recordSize = 0;
    uint32_t delay = 1000;
    
    if (tcp && buf && size) {
        if (tcp->records) {
	
            DEBUGP(DDEBUG, "_tcpRead", "reading %u bytes with records", size);

            if (recv(tcp->sock->skd, &recordSize, sizeof(recordSize), 0) <= 0) {
                DEBUGP(DERR, "_tcpRead", "unable to receive record size");
                return 0;
            }
            
            DEBUGP(DDEBUG, "_tcpRead", "received %u record size", recordSize);
            recordSize = ltohel (recordSize); // was sent to us as Little Endian
            DEBUGP(DDEBUG, "_tcpRead", "converted record size from little endian to host: %u", recordSize);
            
            if (recordSize > size) {
                DEBUGP(DERR, "_tcpRead", "record(%d) is too large for allocated buffer (%d)", recordSize, size);
                return 0;
            }
        }
        
        if (!*buf) {
            dynamicBuffer = TRUE;
            *buf = (char *) calloc(size+1, sizeof(char));
            if(!*buf) {
                DEBUGP (DERR,"_tcpRead","cannot allocate memory for %lu bytes!",(unsigned long)size+1);
                return 0;
            }
            (*buf)[size] = '\0';
        }

        totalReceived = 0;
        do {
            if((returnValue = recv(tcp->sock->skd, *buf+totalReceived, size-totalReceived, 0)) < 0) {
                if(errno == EINTR && !SystemExit) {
                    DEBUGP(DDEBUG, "_tcpRead", "signal interrupted recv() call (%s), retrying", strerror(errno));
                    continue;
                }
                DEBUGP (DERR,"_tcpRead","tcp socket read error (%s)", strerror(errno));
                if(dynamicBuffer) {
                    free(*buf);
                    *buf = NULL;
                }
                totalReceived = 0;
                break;
            } else
                if(returnValue == 0) {
                    DEBUGP(DDEBUG, "_tcpRead", "peer closed the connection");
                    break;		
                }
	    
            totalReceived += returnValue;

            if (!tcp->records) break;

            if(totalReceived < size) {
                DEBUGP(DDEBUG, "_tcpRead", "interrupted recv() (%s), retrying", strerror(errno));
                usleep (delay);
                delay *= 2;
            }
            
        } while(tcp->records && totalReceived < size);

        if (totalReceived && dynamicBuffer) {
            /* re-size the buffer to only contain the amount of data actually read! */
            *buf = realloc (*buf, totalReceived + 1);
            (*buf)[totalReceived] = '\0'; /* safety */
        }
        
    }
    return totalReceived;
}

/*
  Plain TCP send interface. Just write the data and worry
  about nothing.
*/
static uint32_t
_tcpWrite (tcp_t *tcp, char *buf, uint32_t size) {

    int32_t returnValue = 0, totalSent = 0;
    uint32_t delay = 10000;
    
    if(tcp && tcp->sock && buf && size) {

        if (tcp->records) {

            uint32_t tmp_size = htolel (size); // send size as Little Endian

            DEBUGP(DDEBUG, "_tcpWrite", "writing %u bytes with records", size);
            
            if(send(tcp->sock->skd,&tmp_size,sizeof(tmp_size), 0) < 0) {
                DEBUGP (DERR, "_tcpWrite", "error while sending record size");
                return 0;
            }
        }
        
        totalSent = 0;
	
        while(totalSent < size) {
    
            if((returnValue = send(tcp->sock->skd, buf+totalSent, size-totalSent, 0)) < 0) {
                DEBUGP (DERR, "_tcpWrite", "tcp socket write error (errno %d)", errno);
                totalSent = 0;
                break;
	
            } else
                if(returnValue == 0) {
                    DEBUGP (DALL, "_tcpWrite", "peer closed the connection");
                    break;
                }
	    
            totalSent += returnValue;
	    
            if(totalSent < size) {
                DEBUGP (DERR, "_tcpWrite", "interrupted send() (errno %d), retrying", errno);
                usleep (delay);
                delay *= 2;
            }
        }
    }

    return totalSent;
}

IMPLEMENT_INTERFACE (TcpConnector) = {
	.destroy    = _destroy,
	.connect2    = _connect2,
        .connect    = _connect,
	.listen     = _listen,
	.accept     = _accept,
	.read       = _tcpRead,
	.write      = _tcpWrite,
        .useRecords = useRecords,
	.setTimeout = _setTimeout

};

