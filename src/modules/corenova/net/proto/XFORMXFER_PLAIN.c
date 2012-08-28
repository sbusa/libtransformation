#include <corenova/source-stub.h>

THIS = {
	.version     = "2.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description =
	"This module implements XFORMXFER_PLAIN protocol, a hacked version of XFORMXFER",
	.implements  = LIST ("SessionProtocol", "Transformation",
						 /* the following are internal-only */
						 "XformxferEntry","XformxferMap"),
	.requires    = LIST ("corenova.data.list", /* for map implementation */
						 "corenova.data.string", /* for format comparison */
						 "corenova.net.session",
						 "corenova.sys.quark",
                         "corenova.sys.transform"),
    .transforms  = LIST ("net:session:protocol::XFORMXFER_PLAIN -> data:message")
};

#include <corenova/data/list.h>
#include <corenova/data/string.h>
#include <corenova/net/session.h>
#include <corenova/sys/quark.h>
#include <corenova/sys/transform.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <signal.h>
#include <unistd.h>				/* for usleep */

typedef struct {
	
	uint32_t size;
	unsigned char id;
	
} ALIGNED64 xformxfer_hdr_t;

typedef struct {

	unsigned char id;
	char ALIGNED64 *format;
	
} ALIGNED64 xformxfer_entry_t;

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
  process registered xformxfer request

  1. find the entry in the map based on id
  2. retrieve the data upto the size as specified in the header
  3. create a new BinaryObject to encapsulate the data
  4. construct a Message, and add the new BinaryObject as a part of the
  Message.
  5. push the new Message up to the Session layer for further processing.
*/
static boolean_t
_serverProcessRequest (session_t *session, xformxfer_map_t *map, xformxfer_hdr_t *hdr) {
	xformxfer_entry_t *entry = I (XformxferMap)->findById (map,hdr->id);
	if (entry) {
		char *data = NULL;
		uint32_t dataSize = 0;
		DEBUGP (DINFO,"serverProcessRequest","processing xformxfer request for %lu bytes of %s",(unsigned long)hdr->size,entry->format);
		if ((dataSize = I (Transport)->recv (session->transport,&data,hdr->size))) {
			DEBUGP (DDEBUG,"serverProcessRequest","read %lu bytes for %s",(unsigned long)dataSize,entry->format);
			binary_t *binData = I (BinaryObject)->new (data,dataSize,entry->format);
			if (binData) {
				/*
				  received data, construct message and push it up to the
				  session layer.
				*/
				char *peerCname = I (SessionHack)->getPeerCname (session);
				message_t *message = I (Message)->new (0,peerCname,NULL);
				free (peerCname);
				if (message) {
					message_part_t *part = I (MessagePart)->new (MESSAGE_BINARY, binData);
					if (part) {
						if (I (Message)->attachPart (message,part)) {
						
							uint32_t sleep_time = 5000;
							uint32_t retries = 0;

							while(!I (Session)->pushMessage (session,message)) {
							
								if(retries % 100 == 0)
								    DEBUGP (DERR,"serverProcessRequest","unable to push message into session's incoming message queue!");
								    
								usleep(sleep_time);
								retries++;
								if(sleep_time < 50000)
								    sleep_time += 5000;
							}
							
							return TRUE;
							
						} else {
							DEBUGP (DERR,"serverProcessRequest","unable to attach part to the message!");
							I (MessagePart)->destroy (&part);
							I (Message)->destroy (&message);
						}
					} else {
						DEBUGP (DERR,"serverProcessRequest","unable to create a message part!");
						I (Message)->destroy (&message);
						I (BinaryObject)->destroy (&binData);
					}
				} else {
					DEBUGP (DERR,"serverProcessRequest","unable to create a new message object!");
					I (BinaryObject)->destroy (&binData);
				}
			} else {
				DEBUGP (DERR,"serverProcessRequest","unable to wrap incoming data with Binary object!");
				free (data);
			}
		} else {
			DEBUGP (DERR,"serverProcessRequest","unable to receive requested data of '%s'",entry->format);
		}
	} else {
		DEBUGP (DERR,"serverProcessRequest","unregistered xformxfer id %u provided!",hdr->id);
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
			entry = I (XformxferEntry)->new (format,I (List)->count (map) + 1);
			if (entry) {
				if (!I (XformxferMap)->add (map,entry)) {
					DEBUGP (DERR,"serverProcessRegistration","unable to add '%s' to xformxfer map!",format);
					I (XformxferEntry)->destroy (&entry);
					free (format);
					return FALSE;
				}
			} else {
				DEBUGP (DERR,"serverProcessRegistration","unable to create a new xformxfer entry for '%s'!",format);
				free (format);
				return FALSE;
			}
		}
		free (format);
		/* here, send back the header with new info */
		xformxfer_hdr_t confirmation = {
			.id = entry->id,
			.size = strlen (entry->format)
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

/*
  1. read xformxfer header data
  2. if registration request, resolve the registration
  3. process the incoming data item
*/
static boolean_t
_serverLoop (void *inData) {
	session_t *session = (session_t *)inData;
	if (session) {
		if (session->userData) {
			xformxfer_map_t *map = (xformxfer_map_t *)session->userData;
			xformxfer_hdr_t hdr; char *pHdr = (char *)&hdr;
			
			/* read incoming xformxfer header */
			if (I (Transport)->recv (session->transport,&pHdr,sizeof (xformxfer_hdr_t))) {
				if (hdr.id) {
					if (_serverProcessRequest (session, map, &hdr)) {
						return TRUE;
					}
				} else {
					if (_serverProcessRegistration (session, map, &hdr)) {
						return TRUE;
					}
				}
			} else {
				DEBUGP (DERR,"serverLoop","unable to retrieve XFORMXFER header!");
			}
		} else {
			/* initialize runtime data structure */
			session->userData = I (XformxferMap)->new ();
			return TRUE;
		}
		
		/* If we're here, something went wrong! */
		
		I (Transport)->destroy(&session->transport);
		
		if (session->userData) {

                    void **map = &session->userData;
			I (XformxferMap)->destroy ((xformxfer_map_t **)map);

                }

        session->state = SESSION_TERMINATED;
		pthread_kill(session->quark->parent, SIGALRM);
		
	}
	return FALSE;
}

/***** Client Communication *****/

static xformxfer_entry_t *_clientRegistration (session_t *session , xformxfer_map_t *map, const char *format) {
	xformxfer_entry_t *entry = I (XformxferMap)->findByFormat (map,format);
	if (!entry) {
		xformxfer_hdr_t hdr = {
			.size = strlen (format),
			.id = 0
		};
		
		
		if (I (Transport)->send (session->transport,(char *)&hdr, sizeof (hdr))) {
			DEBUGP (DMSG,"clientRegistration","sent registration request header for '%s'", format);
			
			if (I (Transport)->send (session->transport,(char *) format,strlen (format))) {
				DEBUGP (DMSG,"clientRegistration","sent registration format string '%s'", format);
				char *pHdr = (char *)&hdr;
				if (I (Transport)->recv (session->transport, (char**)&pHdr,sizeof (hdr))) {
					DEBUGP (DDEBUG,"clientRegistration","received confirmation from server");
					if (hdr.size == strlen (format)) {
						DEBUGP (DDEBUG,"clientRegistration","confirmation header verified!");
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
	}
	return entry;
} 

static boolean_t _clientLoop (void *inData) {
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
								.size = binary->size,
								.id = entry->id
							};
							
							if (I (Transport)->send (session->transport,(char *)&hdr,sizeof (hdr))) {
								if (I (Transport)->send (session->transport,(char *)binary->data,binary->size)) {
									successParts++;
								}
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
				/* no message to send... */
				usleep (100000);
				return TRUE;
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

		quark_t *quark = NULL;
		switch (session->mode) {
		  case SESSION_SERVER: quark = I (Quark)->new (_serverLoop,session); break;
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

enum xform_type {
    PROTOCOL2MESSAGE = 1
};

static transformation_t *
_newProtocolTransformation (const char *from, const char *to, parameters_t *blueprint) {
    transformation_t * xform = (transformation_t *)calloc (1, sizeof (transformation_t));
    if (xform) {
        if (from && to && blueprint) {
            /* validate from & to */
            if (I (String)->equal (from,"net:session:protocol::XFORMXFER_PLAIN") &&
                I (String)->equal (to,  "data:message")) {
                xform->type = PROTOCOL2MESSAGE;
            }
            else {
                DEBUGP (DERR,"_newProtocolTransformation", "transformation %s -> %s is not supported!", from, to);
                free (xform);
                return NULL;
            }
            
            /* proper exit */
            xform->module = SELF;
            xform->blueprint = I (Parameters)->copy (blueprint);
            xform->from = strdup (from);
            xform->to   = strdup (to);
        }
    }
    return xform;
}

static transform_object_t *
_executeProtocolTransformation (transformation_t *xform, transform_object_t *in) {
    /*
     * assumes that all structures inside XFORM has been properly initialized.
     */
	if (xform) {
        switch (xform->type) {
          case PROTOCOL2MESSAGE: {
              /*
               * conversion from net:session:protocol:XFORMXFER_PLAIN into data:message
               * 
               * does NOT depend on XFORM->INSTANCE
               */ 
              session_protocol_t *sessproto = (session_protocol_t *)in->data;
              if (sessproto) {
                  session_t *session = sessproto->session;
                  protocol_t *protocol = sessproto->protocol;
                  if (session && protocol) {
                      xformxfer_map_t *map = (xformxfer_map_t *)session->userData;
                      if (!map) {
                          map = session->userData = I (XformxferMap)->new ();
                      }
		      
                    I (Transport)->forcerawtcp(session->transport); // fall back to raw TCP communication
                    I (Transport)->useRecords (session->transport, TRUE); // the server will expect this!

                    read_header:
                      switch (session->mode) {
                        case SESSION_SERVER: {
                            xformxfer_hdr_t hdr; char *pHdr = (char *)&hdr;

                            if (I (Transport)->recv (session->transport,&pHdr,sizeof (xformxfer_hdr_t))) {
                                if (hdr.id) {
                                    if (_serverProcessRequest (session, map, &hdr)) {
                                        message_t *message = I (Session)->recvMessage (session);
                                        if (message) {
                                            transform_object_t *obj = I (TransformObject)->new ("data:message",message);
                                            if (obj) {
                                                obj->destructor = SELF;
                                                obj->destroy = (XDESTROY) I (Message)->destroy;
                                                return obj;
                                            }
                                            I (Message)->destroy (&message);
                                        }
                                    }
                                } else {
                                    if (_serverProcessRegistration (session, map, &hdr)) goto read_header;
                                }
                            }
                            break;
                        }
                        case SESSION_CLIENT: {
                            DEBUGP (DERR,"_executeProtocolTransformation","XFORMXFER_PLAIN does not support retrieving of session message from protocol!");
                            break;
                        }
                      }
                  }
              }
              break;
          }
        }
    }
	return NULL;
}

static void
_destroyProtocolTransformation (transformation_t **tPtr) {
	if (tPtr) {
        transformation_t *xform = *tPtr;
        if (xform) {
            I (Parameters)->destroy (&xform->blueprint);
            free (xform->from);
            free (xform->to);
            free (xform);
            *tPtr = NULL;
        }
	}
}

/*
 * XXX - does this belong here???
 */
static void
_freeProtocolTransformObject (transform_object_t *obj) {
	if (obj && obj->data) {
        if (I (String)->equal (obj->format,"data:message")) {
            message_t *message = (message_t *)obj->data;
            I (Message)->destroy (&message);
            obj->data = NULL;
        }
    }
}

IMPLEMENT_INTERFACE (Transformation) = {
	.new       = _newProtocolTransformation,
	.destroy   = _destroyProtocolTransformation,
	.execute   = _executeProtocolTransformation,
	.free      = _freeProtocolTransformObject
};

