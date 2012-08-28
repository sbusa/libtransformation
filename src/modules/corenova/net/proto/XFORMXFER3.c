#include <corenova/source-stub.h>

THIS = {
	.version     = "2.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description =
	"This module implements XFORMXFER3 protocol, an enhanced version of XFORMXFER2_PLAIN",
	.implements  = LIST ("SessionProtocol", "Transformation",
						 /* the following are internal-only */
						 "XformxferEntry","XformxferMap"),
	.requires    = LIST ("corenova.data.list", /* for map implementation */
						 "corenova.data.string", /* for format comparison */
                         "corenova.data.array",
						 "corenova.net.session",
						 "corenova.sys.quark",
						 "corenova.sys.transform"),
						 
	.transforms  = LIST ("net:session:protocol::XFORMXFER3 -> data:message")
	
};

#include <corenova/data/list.h>
#include <corenova/data/array.h>
#include <corenova/data/string.h>
#include <corenova/net/session.h>
#include <corenova/sys/quark.h>
#include <corenova/sys/transform.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <signal.h>
#include <unistd.h>				/* for usleep */

typedef struct {
	
	unsigned char id;
	uint32_t size;
	uint32_t csum;
	
} ALIGNED64 xformxfer_hdr_t;

typedef struct {

	unsigned char id;
	char ALIGNED64 *format;
	
} ALIGNED64 xformxfer_entry_t;

#define XFORMXFER_DEFAULT_TIMEOUT 1000

typedef enum {

    XFORMXFER_REGISTRATION = 0,
    XFORMXFER_HELLO,
    XFORMXFER_CONFIRMATION
    
} xformxfer_req_t;

#define XFORMXFER_RESERVED_REQUESTS_OFFSET 10


/*
  Internal interface for use by this module only!
*/
DEFINE_INTERFACE (XformxferEntry) {
	xformxfer_entry_t *(*new)     (const char *format, unsigned char id);
	void               (*destroy) (xformxfer_entry_t **);
};

/***** XformxferEntry Interface (INTERNAL) *****/

static xformxfer_entry_t *
_newXformxferEntry (const char *format, unsigned char id) {
	xformxfer_entry_t *entry = (xformxfer_entry_t *)calloc (1,sizeof (xformxfer_entry_t));
	if (entry) {
		entry->id     = id;
		entry->format = I (String)->copy (format);
	}
	return entry;
}

static void
_destroyXformxferEntry (xformxfer_entry_t **entryPtr) {
	if (entryPtr) {
		xformxfer_entry_t *entry = *entryPtr;
		if (entry) {
			free (entry->format);
			free (entry);
			*entryPtr = NULL;
		}
	}
}

IMPLEMENT_INTERFACE (XformxferEntry) = {
	.new     = _newXformxferEntry,
	.destroy = _destroyXformxferEntry
};

typedef list_t      xformxfer_map_t;
typedef list_item_t xformxfer_map_entry_t;

/*
  Internal interface for use by this module only!
*/
DEFINE_INTERFACE (XformxferMap) {
	xformxfer_map_t   *(*new)          (void);
	boolean_t          (*add)          (xformxfer_map_t *, xformxfer_entry_t *);
	xformxfer_entry_t *(*findById)     (xformxfer_map_t *, uint8_t id);
	xformxfer_entry_t *(*findByFormat) (xformxfer_map_t *, const char *format);
	void               (*destroy)      (xformxfer_map_t **);
};

/***** XformxferMap Interface (INTERNAL) *****/

static xformxfer_map_t *
newXformxferMap () {
	return (xformxfer_map_t *) I (List)->new ();
}

static boolean_t
addXformxferEntry (xformxfer_map_t *map, xformxfer_entry_t *xEntry) {
	if (I (List)->insert (map, I (ListItem)->new (xEntry))) {
		return TRUE;
	}
	return FALSE;
}

static xformxfer_entry_t *
findXformxferEntryById (xformxfer_map_t *map, uint8_t id) {
	if (map) {
		xformxfer_map_entry_t *entry = I (List)->first (map);
		while (entry) {
			xformxfer_entry_t *xEntry = (xformxfer_entry_t *) entry->data;
			if (xEntry && xEntry->id == id)
				return xEntry;
			
			entry = I (List)->next (entry);
		}
	}
	return NULL;
}

static xformxfer_entry_t *
findXformxferEntryByFormat (xformxfer_map_t *map, const char *format) {
	if (map) {
		xformxfer_map_entry_t *entry = I (List)->first (map);
		while (entry) {
			xformxfer_entry_t *xEntry = (xformxfer_entry_t *) entry->data;
			if (xEntry && I (String)->equal (xEntry->format, format))
				return xEntry;
			
			entry = I (List)->next (entry);
		}
	}
	return NULL;
}


static void
destroyXformxferMap (xformxfer_map_t **mapPtr) {
	if (mapPtr) {
		xformxfer_map_t *map = *mapPtr;
		if (map) {
			xformxfer_map_entry_t *entry = NULL;
			while ((entry = I (List)->pop (map))) {
				xformxfer_entry_t *xEntry = (xformxfer_entry_t *) entry->data;
				I (XformxferEntry)->destroy (&xEntry);
			}
//			free (map);			/* this should suffice */
//			*mapPtr = NULL;
			I (List)->destroy((list_t**)mapPtr);
		}
	}
}

IMPLEMENT_INTERFACE (XformxferMap) = {
	.new          = newXformxferMap,
	.add          = addXformxferEntry,
	.findById     = findXformxferEntryById,
	.findByFormat = findXformxferEntryByFormat,
	.destroy      = destroyXformxferMap
};

/***** Server Communication *****/

/*
  process registered xformxfer message

  1. find the entry in the map based on id
  2. retrieve the data upto the size as specified in the header
  3. create a new BinaryObject to encapsulate the data
  4. construct a Message, and add the new BinaryObject as a part of the
  Message.
  5. push the new Message up to the Session layer for further processing.
*/
static boolean_t
_serverProcessMessage (session_t *session, xformxfer_map_t *map, xformxfer_hdr_t *hdr) {
	xformxfer_entry_t *entry = I (XformxferMap)->findById (map,hdr->id);
	if (entry) {
		char *data = NULL;
		uint32_t dataSize = 0;
		DEBUGP (DINFO,"serverProcessMessage","processing xformxfer request for %lu bytes of %s",(unsigned long)hdr->size,entry->format);
		if ((dataSize = I (Transport)->recv (session->transport,&data,hdr->size)) && I (String)->crc32(data, hdr->size) == hdr->csum) {
		
			DEBUGP (DDEBUG,"serverProcessMessage","read %lu bytes for %s",(unsigned long)dataSize,entry->format);
			binary_t *binData = I (BinaryObject)->new (data,dataSize,entry->format);
			if (binData) {
				/*
				  received data, construct message and push it up to the
				  session layer.
				*/
                DEBUGP (DDEBUG,"serverProcessMessage","read %lu bytes for %s - made new bindata @ %p",(unsigned long)dataSize,entry->format,binData);
				message_t *message = I (Message)->new (0,session->from,NULL);
				if (message) {
					message_part_t *part = I (MessagePart)->new (MESSAGE_BINARY, binData);
					if (part) {
						if (I (Message)->attachPart (message,part)) {
						
							uint32_t sleep_time = 5000;
							uint32_t retries = 0;

							while(!I (Session)->pushMessage (session,message)) {
							
								if(retries % 100 == 0)
								    DEBUGP (DERR,"serverProcessMessage","unable to push message into session's incoming message queue!");
								    
								usleep(sleep_time);
								
								retries++;
								
								if(sleep_time < 50000)
								    sleep_time += 5000;
							}
							return TRUE;
							
						} else {
							DEBUGP (DERR,"serverProcessMessage","unable to attach part to the message!");
							I (MessagePart)->destroy (&part);
							I (Message)->destroy (&message);
						}
					} else {
						DEBUGP (DERR,"serverProcessMessage","unable to create a message part!");
						I (Message)->destroy (&message);
						I (BinaryObject)->destroy (&binData);
					}
				} else {
					DEBUGP (DERR,"serverProcessMessage","unable to create a new message object!");
					I (BinaryObject)->destroy (&binData);
				}
			} else {
				DEBUGP (DERR,"serverProcessMessage","unable to wrap incoming data with Binary object!");
				free (data);
			}
		} else {
			DEBUGP (DERR,"serverProcessMessage","unable to receive requested data of '%s'",entry->format);
		}
	} else {
		DEBUGP (DERR,"serverProcessMessage","unregistered xformxfer id %u provided!",hdr->id);
	}
	return FALSE;
}

/*
  handle registration

  1. read the format string.
  2. check if there's entry in XformxferMap already
  3. send back a response with new id and size of format string
*/
static boolean_t
_serverProcessRegistration (session_t *session, xformxfer_map_t *map, xformxfer_hdr_t *hdr) {
	char *format = NULL;
	uint32_t dataSize = 0;
	DEBUGP (DMSG,"serverProcessRegistration","client requesting registration of xformxfer transfer!");
	
	if ((dataSize = I (Transport)->recv (session->transport,&format,hdr->size))) {
		DEBUGP (DINFO,"serverProcessRegistration","read %lu/%lu bytes %s (%u)",(unsigned long)dataSize,(unsigned long)hdr->size,format,(unsigned int)strlen (format));
		xformxfer_entry_t *entry = I (XformxferMap)->findByFormat (map,format);
		if (!entry) {
            DEBUGP (DERR,"serverProcessRegistration","registeration request for '%s' cannot be completed! (not supported)",format);
            free (format);
            return FALSE;
		}
		free (format);
		/* here, send back the header with new info */
		
		size_t size = strlen (entry->format);
		
		xformxfer_hdr_t confirmation = {
			.id = entry->id,
			.size = htolel (size),
			.csum = I (String)->crc32(entry->format, size)
		};
		
		if (I (Transport)->send (session->transport,(char *)&confirmation,sizeof (confirmation))) {
			DEBUGP (DMSG,"serverProcessRegistration","sent registration confirmation for '%s' (%u,%lu).",
					entry->format,confirmation.id,(unsigned long)confirmation.size);
			return TRUE;
		} else {
			DEBUGP (DERR,"serverProcessRegistration","unable to transmit registration confirmation for '%s'!",entry->format);
		}
	} else {
		DEBUGP (DERR,"serverProcessRegistration","unable to read format string");
	}
	return FALSE;
}

/***** Client Communication *****/

static xformxfer_entry_t *
_clientRegistration (session_t *session , xformxfer_map_t *map, const char *format) {
	xformxfer_entry_t *entry = I (XformxferMap)->findByFormat (map,format);
	if (!entry) {
	
		size_t size = strlen(format);
	
		xformxfer_hdr_t hdr = {
			.id = XFORMXFER_REGISTRATION,
			.size = htolel (size),
			.csum = htolel (I (String)->crc32((char *)format, size))
		};
		
		if (I (Transport)->send (session->transport,(char *)&hdr, sizeof (hdr))) {
			DEBUGP (DMSG,"clientRegistration","sent registration request header for '%s'", format);
			
			if (I (Transport)->send (session->transport,(char *) format, size)) {
				DEBUGP (DMSG,"clientRegistration","sent registration format string '%s'", format);
				char *pHdr = (char *)&hdr;
				if (I (Transport)->recv (session->transport, (char**)&pHdr,sizeof (hdr))) {
					DEBUGP (DDEBUG,"clientRegistration","received confirmation from server");
					if (ltohel(hdr.size) == size) {
						DEBUGP (DDEBUG,"clientRegistration","confirmation header verified, received unique ID: %u",hdr.id);
						entry = I (XformxferEntry)->new (format, hdr.id);
						if (!I (XformxferMap)->add (map,entry)) {
							DEBUGP (DERR,"clientRegistration","unable to add new entry into map");
							I (XformxferEntry)->destroy (&entry);
						}
					} else {
					    DEBUGP (DERR,"clientRegistration","confirmation header format size mismatch!");
					}
				} else {
					DEBUGP (DERR,"clientRegistration","unable to retrieve confirmation from server!");
				}
			} else {
				DEBUGP (DERR,"clientRegistration","unable to transmit format string!");
			}
		} else {
			DEBUGP (DERR,"clientRegistration","unable to transmit registration request header!");
		}
        DEBUGP(DDEBUG, "clientRegistration", "done with registration");
	}
	
	return entry;
} 

#define XFORMXFER_IDLE_USLEEP 50000
#define XFORMXFER_HELLO_INTERVAL 10 /* every 10 seconds */

static long int lastHello = 0;

static boolean_t
_clientLoop (void *inData) {
	session_t *session = (session_t *)inData;
	if (session) {

		if (session->userData) {

			xformxfer_map_t *map = (xformxfer_map_t *)session->userData;
			message_t *message = I (Session)->popMessage (session);
			
			if (message) {
				message_part_t *part = NULL;
				unsigned char messageParts   = I (Message)->countParts (message);
				unsigned char successParts = 0;
				unsigned char idx = 0;

				while ((part = I (Message)->getPart (message,idx++))) {
					if (part->encoding == MESSAGE_BINARY) {
						binary_t *binary = part->content.binary;
						xformxfer_entry_t *entry = _clientRegistration (session, map, binary->format);
						if (entry) {
							xformxfer_hdr_t hdr = {
								.id = entry->id,
								.size = htolel(binary->size),
								.csum = htolel(I (String)->crc32((char *)binary->data, binary->size))
							};
							
							if (I (Transport)->send (session->transport,(char *)&hdr,sizeof (hdr))) {
							
								DEBUGP(DDEBUG, "clientLoop", "sent message header");
								if (I (Transport)->send (session->transport,(char *)binary->data, binary->size)) {
								    DEBUGP(DDEBUG, "clientLoop", "sent message data");
                                    successParts++;
								} else {
								    DEBUGP(DERR, "clientLoop", "failed to send message data");
                                    break;
								}
							} else {
							    DEBUGP(DERR, "clientLoop", "failed to send header");
							}
						} else {
							DEBUGP (DERR,"clientLoop","unable to register %s",binary->format);
							break;
						}
					}
				}

				I (Message)->destroy (&message); /* consume! */
				
				if (successParts == messageParts) {
				    return TRUE;
				}
				
			} else {
				/*
                 * no message to send...
                 *
                 * Take the opportunity to perform some HELLO handshake (in future CONFIRMATION checks?)
                 *
                 */
                long int tdiff = __tsec () - lastHello;
                if (tdiff > XFORMXFER_HELLO_INTERVAL) {
                    xformxfer_hdr_t hello = {
                        .id = XFORMXFER_HELLO,
                        .size = 0,
                        .csum = tdiff
                    };
                    xformxfer_hdr_t confirmation;
                    char *pconfirm = (char *)&confirmation;

                    DEBUGP(DDEBUG, "clientLoop", "sending hello request to server");
                    if (I (Transport)->send (session->transport, (char *)&hello, sizeof (hello))) {
                        if(I (Transport)->recv(session->transport, (char**)&pconfirm, sizeof(confirmation))) {
							
                            if(memcmp(&hello, &confirmation, sizeof(confirmation)) == 0) {
                                DEBUGP(DDEBUG, "clientLoop", "received hello confirmation");
                                lastHello = __tsec ();
                                return TRUE;
                            } else {
                                DEBUGP(DERROR, "clientLoop", "invalid hello confirmation received");
                            }
                            
                        } else {
                            DEBUGP(DERROR, "clientLoop", "unable to receive hello confirmation");
                        }
                    } else {
                        DEBUGP (DERROR, "clientLoop", "unable to send hello request");
                    }
                } else {
                    DEBUGP(DINFO, "clientLoop", "nothing to send, %d seconds since last Hello transaction",tdiff);
                    usleep (XFORMXFER_IDLE_USLEEP);
                    return TRUE;
                }
			}
		} else {
			session->userData = I (XformxferMap)->new ();
			return TRUE;
		}

		/* if we're here, something went wrong */
		if (session->userData) {
            void **map = &session->userData;
			I (XformxferMap)->destroy ((xformxfer_map_t **)map);
        }

		session->state = SESSION_TERMINATED;
	}
	return FALSE;
}


/***** SessionProtocol Interface Implementation *****/

static boolean_t
startCommunication (session_t *session) {
	DEBUGP (DINFO,"startCommunication","entering with session at %p",session);
	if (session) {
		DEBUGP (DDEBUG, "startCommunication", "falling back to raw TCP communication");
		I (Transport)->forcerawtcp(session->transport);	// fall back to raw TCP communication
        I (Transport)->useRecords (session->transport, TRUE); // the server will expect this!
        
		quark_t *quark = NULL;
		switch (session->mode) {
//		  case SESSION_SERVER: quark = I (Quark)->new (_serverLoop,session); break;
		  case SESSION_CLIENT: quark = I (Quark)->new (_clientLoop,session); break;
		  default:
			  DEBUGP (DERR,"startCommunication","unsupported session mode!");
		}
		DEBUGP (DINFO,"startCommunication","loading quark at %p",quark);
		if (quark) {
		
		    I (Quark)->setname(quark, "startCommunication");

		    if(I (Quark)->spin (quark)) {
                session->state = SESSION_ACTIVE;
                session->quark = quark;
                return TRUE;
		    }
		}
	}
	return FALSE;
}

static void
stopCommunication (session_t *session) {
	DEBUGP (DDEBUG,"stopCommunication","shutting down the protocol layer...");
	if (session) {
		if (session->quark)
			I (Quark)->destroy (&session->quark);
		if (session->userData) {

                    void **map = &session->userData;
			I (XformxferMap)->destroy ((xformxfer_map_t **)map);
                }
	}
	DEBUGP (DDEBUG,"stopCommunication","quark destroyed and user-space data structures flushed!");
}

IMPLEMENT_INTERFACE (SessionProtocol) = {
	.start = startCommunication,
	.stop  = stopCommunication
};

/*//////// Transformation Interface Implementation //////////////////////////////////////////*/

TRANSFORM_EXEC (protocol2message) {
    /*
     * conversion from net:session:protocol:XFORMXFER2_PLAIN into data:message or data:message:array
     *
     * depends on XFORM->INSTANCE for XFORMXFER MAP
     *
     * NOTE: data:message:array NOT YET implemented!
     */

    session_t *session = (session_t *)in->data;
    xformxfer_map_t *map = (xformxfer_map_t *)xform->instance;
    if (session && map) {

        int timeout = I (Parameters)->getTimeValue (xform->blueprint,"xformxfer_timeout");
        if (timeout < 0)
            timeout = XFORMXFER_DEFAULT_TIMEOUT;

        I (Transport)->forcerawtcp(session->transport); // fall back to raw TCP communication
        I (Transport)->useRecords (session->transport, TRUE);

      read_header:
        switch (session->mode) {
          case SESSION_SERVER: {
              xformxfer_hdr_t hdr; char *pHdr = (char *)&hdr;
              
              switch (I (Transport)->poll (session->transport, TRANSPORT_POLLIN, timeout)) {
                case TRANSPORT_TIMEOUT: {
                    transform_object_t *obj = I (TransformObject)->new (xform->to,NULL);
                    return obj;
                }
                case TRANSPORT_FATAL:
                    DEBUGP (DWARN,"protocol2message","transport error detected!");
                    return NULL;
              }


              /*
               * protocol transaction ALWAYS starts on server end by reading the header
               */
              DEBUGP(DDEBUG, "protocol2message", "receiving xformxfer header (%u) bytes", sizeof (xformxfer_hdr_t));

              if (I (Transport)->recv (session->transport,&pHdr,sizeof (xformxfer_hdr_t))) {
                  transform_object_t *obj = NULL;
                  DEBUGP(DDEBUG, "protocol2message", "hdr.id = %hhu", hdr.id);

                  switch (hdr.id) {
                    case XFORMXFER_REGISTRATION: if (_serverProcessRegistration (session, map, &hdr)) goto read_header;
                        break;
                    case XFORMXFER_HELLO:
                        DEBUGP(DDEBUG, "protocol2message", "sending hello response to client");
                        if (!I (Transport)->send (session->transport,pHdr,sizeof (xformxfer_hdr_t))) {
                            DEBUGP (DERROR,"_serverProcessMessage","unable to respond to HELLO");
                            return FALSE;
                        }

                        DEBUGP (DDEBUG,"protocol2message","returning dummy object");
                        obj = I (TransformObject)->new (xform->to,NULL);
                        return obj;
                        
                    case XFORMXFER_CONFIRMATION:
                        /*
                         * TODO - Should have special logic to provide confirmation services (in future)
                         */ 
                        DEBUGP(DDEBUG, "protocol2message", "sending confirmation response to client");
                        if (!I (Transport)->send (session->transport,pHdr,sizeof (xformxfer_hdr_t))) {
                            DEBUGP (DERROR,"_serverProcessMessage","unable to confirm request");
                            return FALSE;
                        }

                        DEBUGP (DDEBUG,"protocol2message","returning dummy object");
                        obj = I (TransformObject)->new (xform->to,NULL);
                        return obj;
                        
                    default:
                      if (_serverProcessMessage (session, map, &hdr)) {
                          message_t *message = I (Session)->recvMessage (session);
                          if (message) {
                              transform_object_t *obj = I (TransformObject)->new (xform->to,message);
                              if (obj) {
                                  obj->destructor = SELF;
                                  obj->destroy = (XDESTROY) I (Message)->destroy;
                                  // XXX - saint - this message does not need to be protected!
                                  //I (TransformObject)->save (obj); /* special logic to prevent previous transform object's data from being destroyed */

                                  DEBUGP (DDEBUG,"protocol2message","received message @ %p", message);
                                  return obj;
                              }
                              I (Message)->destroy (&message);
                          }
                      }
                      break;
                  }
              }
			    
              DEBUGP(DWARN, "protocol2message", "failed to get xformxfer header");
              break;
          }
          case SESSION_CLIENT: {
              DEBUGP (DERR,"protocol2message","XFORMXFER3 does not support retrieving of session message from protocol!");
              break;
          }
        }
    }
    return NULL;
}

TRANSFORM_NEW (newProtocolTransformation) {

    TRANSFORM ("net:session:protocol::XFORMXFER3","data:message",protocol2message);

    IF_TRANSFORM (protocol2message) {
        TRANSFORM_HAS_PARAM ("xformxfer_formats");
        xformxfer_map_t *map = I (XformxferMap)->new ();
        if (map) {
            list_t *formats = I (String)->tokenize (I (Parameters)->getValue (blueprint,"xformxfer_formats"),",");
            if (formats) {
                list_item_t *item = NULL;
                while ((item = I (List)->pop (formats))) {
                    char *format = (char *)item->data;
                    if (format) {
                        char *sformat = I (String)->trim (format);
                        xformxfer_entry_t *entry = I (XformxferEntry)->new (sformat,I (List)->count (map) + XFORMXFER_RESERVED_REQUESTS_OFFSET);
                        if (entry) {
                            if (!I (XformxferMap)->add (map,entry)) {
                                 DEBUGP (DERR,"newProtocolTransformation","unable to add '%s' to xformxfer map!",sformat);
                            }
                        }
                        free (format);
                    }
                    I (ListItem)->destroy (&item);
                }
                I (List)->clear (formats,TRUE);
                I (List)->destroy (&formats);
            }
            TRANSFORM_WITH (map);
        }
    }
    
} TRANSFORM_NEW_FINALIZE;

TRANSFORM_DESTROY (destroyProtocolTransformation) {

    IF_TRANSFORM (protocol2message) {
        xformxfer_map_t *instance = (xformxfer_map_t *)xform->instance;
        I (XformxferMap)->destroy (&instance);
    }

} TRANSFORM_DESTROY_FINALIZE;

IMPLEMENT_INTERFACE (Transformation) = {
	.new       = newProtocolTransformation,
	.destroy   = destroyProtocolTransformation,
	.execute   = NULL,
	.free      = NULL
};
