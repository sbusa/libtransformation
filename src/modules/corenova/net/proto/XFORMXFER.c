#include <corenova/source-stub.h>

THIS = {
	.version     = "2.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "This module implements XFORMXFER protocol exchange between two endpoints.",
	.implements  = LIST ("SessionProtocol", "Transformation", "XformxferEntry","XformxferMap"),
	.requires    = LIST ("corenova.data.list", "corenova.data.string", "corenova.net.session", "corenova.sys.quark", "corenova.sys.transform"),
	.transforms  = LIST ("net:session:protocol::XFORMXFER -> data:message")
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
	xformxfer_entry_t *(*findById)     (xformxfer_map_t *, uint32_t id);
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
findXformxferEntryById (xformxfer_map_t *map, uint32_t id) {
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
                DEBUGP (DDEBUG,"serverProcessRequest","read %lu bytes for %s - made new bindata @ %p",(unsigned long)dataSize,entry->format,binData);
				char *peerCname = I (SessionHack)->getPeerCname (session);
				message_t *message = I (Message)->new (0,peerCname,NULL);
				free (peerCname);
				if (message) {
					message_part_t *part = I (MessagePart)->new (MESSAGE_BINARY, binData);
					if (part) {
						if (I (Message)->attachPart (message,part)) {
							if (I (Session)->pushMessage (session,message)) {
								DEBUGP (DMSG,"serverProcessRequest","pushing message '%s' (%lu bytes) up to session layer",
										binData->format,(unsigned long)binData->size);
								return TRUE;
							}
							else {
								DEBUGP (DERR,"serverProcessRequest","unable to push message into session's incoming message queue!");
								I (Message)->destroy (&message);
							}
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
            DEBUGP (DERR,"serverProcessRegistration","registeration request for '%s' cannot be completed! (not supported)",format);
            free (format);
            return FALSE;
		}
		free (format);
		/* here, send back the header with new info */
		xformxfer_hdr_t confirmation = {
			.id = entry->id,
			.size = strlen (entry->format)
		};
		
		if (I (Transport)->send (session->transport, (char*)&confirmation,sizeof (confirmation))) {
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

static xformxfer_entry_t *_clientRegistration (session_t *session , xformxfer_map_t *map, const char *format) {
	xformxfer_entry_t *entry = I (XformxferMap)->findByFormat (map,format);
	if (!entry) {
		xformxfer_hdr_t hdr = {
			.id = 0,
			.size = strlen (format)
		};

		if (I (Transport)->send (session->transport, (char *)&hdr, sizeof (hdr))) {
			DEBUGP (DMSG,"clientRegistration","sent registration request header for '%s'", format);
			if (I (Transport)->send (session->transport, (char*)format,strlen (format))) {
				DEBUGP (DMSG,"clientRegistration","sent registration format string '%s'", format);
				char *pHdr = (char *)&hdr;
				if (I (Transport)->recv (session->transport,(char**)&pHdr,sizeof (hdr))) {
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
								.id = entry->id,
								.size = binary->size
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
		pthread_kill(session->quark->parent, SIGALRM);
	}
	return FALSE;
}


/***** XXX - deprecated - SessionProtocol Interface Implementation *****/

static boolean_t
startCommunication (session_t *session) {
	DEBUGP (DINFO,"startCommunication","entering with session at %p",session);
	if (session) {
		quark_t *quark = NULL;
		switch (session->mode) {
//          case SESSION_SERVER: quark = I (Quark)->new (_serverLoop,session); break;
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

TRANSFORM_EXEC (xformxfer_to_message) {
    /*
     * conversion from net:session:protocol::XFORMXFER2_PLAIN into data:message
     * 
     * depends on XFORM->INSTANCE for XFORMXFER MAP
     */ 
    session_t *session = (session_t *)in->data;
    xformxfer_map_t *map = (xformxfer_map_t *)xform->instance;
    if (session && map) {
//		    I (Transport)->setTimeout(session->transport, 30);
            
      read_header:
        switch (session->mode) {
          case SESSION_SERVER: {
              xformxfer_hdr_t hdr; char *pHdr = (char *)&hdr;

              switch (I (Transport)->poll (session->transport, TRANSPORT_POLLIN, 30000)) {
                case TRANSPORT_TIMEOUT: {
                    transform_object_t *obj = I (TransformObject)->new ("data:message",NULL);
                    return obj;
                }
                case TRANSPORT_FATAL:
                    DEBUGP (DWARN,"protocol2message","transport error detected!");
                    return NULL;
              }
                  
              DEBUGP(DDEBUG, "protocol2message", "receiving xformxfer header (%u) bytes", sizeof (xformxfer_hdr_t));
              if (I (Transport)->recv (session->transport,&pHdr,sizeof (xformxfer_hdr_t))) {

                  DEBUGP(DDEBUG, "protocol2message", "hdr.id = %hhu", hdr.id);
			    
                  if (hdr.id) {
                      if (_serverProcessRequest (session, map, &hdr)) {
                          message_t *message = I (Session)->recvMessage (session);
                          if (message) {
                              transform_object_t *obj = I (TransformObject)->new ("data:message",message);
                              if (obj) {
                                  obj->destroy = (XDESTROY) I (Message)->destroy;
                                  // XXX - saint - this message does not need to be protected!
                                  //I (TransformObject)->save (obj); /* special logic to prevent previous transform object's data from being destroyed */
                                  DEBUGP (DDEBUG,"protocol2message","received message @ %p", message);
                                  return obj;
                              }
                              I (Message)->destroy (&message);
                          }
                      }
                  } else {
                      if (_serverProcessRegistration (session, map, &hdr)) goto read_header;
                  }
              }
			    
              DEBUGP(DWARN, "protocol2message", "failed to get xformxfer header");
			    
              break;
          }
          case SESSION_CLIENT: {
              DEBUGP (DERR,"protocol2message","XFORMXFER2_PLAIN does not support retrieving of session message from protocol!");
              break;
          }
        }
    }
    return NULL;
}

TRANSFORM_NEW (newProtocolTransformation) {

    TRANSFORM ("net:session:protocol::XFORMXFER","data:message",xformxfer_to_message);

    IF_TRANSFORM (xformxfer_to_message) {
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
                        xformxfer_entry_t *entry = I (XformxferEntry)->new (sformat,I (List)->count (map) + 1);
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

    IF_TRANSFORM (xformxfer_to_message) {
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
