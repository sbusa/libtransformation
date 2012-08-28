#include <corenova/source-stub.h>

THIS = {
	.version     = "3.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description =
	"This module enables session-oriented network operations.\n"
	"All data transport logic is encapsulated in a PROTOCOL module.\n"
	"Primary mechanism for session communication is via the MESSAGE object, "
	"the application logic uses the established SESSION to send & receive MESSAGEs "
	"across the established SESSION TRANSPORT.",
	.implements  = LIST ("Session","SessionTable","SessionHack","Transformation"),
	.requires    = LIST ("corenova.data.configuration",
						 "corenova.data.list",
						 "corenova.data.message",
						 "corenova.data.queue",
						 "corenova.net.protocol",
						 "corenova.net.transport",
                         "corenova.sys.transform"),
    .transforms = LIST ("net:transport -> net:session::server",
                        "net:transport -> net:session::client",
                        "net:session::server -> net:session:protocol::*",
                        "net:session::client -> net:session:protocol::*")
};

#include <corenova/net/session.h>
#include <corenova/sys/transform.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

/***** SessionHack Interface Implementation *****/

static char *getPeerCname (session_t *session) {
	if (session) {
		transport_t *transport = session->transport;
		if (transport) {
//			if (transport->type == TRANSPORT_SSL) {
            ssl_t *ssl = transport->connection.ssl;
            if (ssl) {
                return I (SSLConnector)->getPeerCname (ssl);
            }
//			}
		}
	}
	return NULL;
}

IMPLEMENT_INTERFACE (SessionHack) = {
	.getPeerCname = getPeerCname
};

/***** Session Interface Implementation *****/

#define DEFAULT_SESSION_OUTGOING_TIMEOUT 100 /* timeout every 100ms */

static session_t *
newSession (enum session_mode mode, transport_t *transport, int insize, int outsize) {
	session_t *session = (session_t *)calloc (1,sizeof (session_t));
	if (session) {
		session->mode = mode;
		/* now make incoming & outgoing data queues */
		session->outgoingMessages =
			I (MessageQueue)->new (insize>0?insize:DEFAULT_SESSION_QUEUE_MAXSIZE);
		session->incomingMessages =
			I (MessageQueue)->new (outsize>0?outsize:DEFAULT_SESSION_QUEUE_MAXSIZE);

		if (session->outgoingMessages && session->incomingMessages) {
			if (transport) {
				session->transport = transport;
                if (transport->connection.ssl)
                    session->from = I (SSLConnector)->getPeerCname (transport->connection.ssl);
			}

            /* make outgoingMessages Queue NON-BLOCKING */
            I (Queue)->setBlocking (session->outgoingMessages, FALSE);
            I (Queue)->setTimeout (session->outgoingMessages, DEFAULT_SESSION_OUTGOING_TIMEOUT);
            
		} else {
			DEBUGP (DERR,"new","failed to create Message Queues!");
			I (Session)->destroy (&session);
		}
	} else {
	
//	    I (Session)->destroy (&session);
	    if(session)
            free(session);
	    session = NULL;
	
	}
	
	return session;
}

static void
destroySession (session_t **sessionPtr){

	if (sessionPtr) {
		session_t *session = *sessionPtr;
		if (session) {
			I (MessageQueue)->destroy (&session->incomingMessages);
			I (MessageQueue)->destroy (&session->outgoingMessages);

            if (session->from) free (session->from);
            if (session->protocol) free (session->protocol);
            
			free (session);
			*sessionPtr = NULL;
		}
	}
}

static char *
_autosync (session_t *session, const char *protocols) {
    char *protoChoice = NULL;

	if (session && protocols) {
        switch (session->mode) {
			
          case SESSION_SERVER:
              DEBUGP (DINFO,"autosync","sending supported protocols [%s]",protocols);
				
              if (I (Transport)->send (session->transport, (char*)protocols, strlen (protocols))) {
                  uint32_t read = I (Transport)->recv (session->transport, &protoChoice, PROTOCOL_NAME_MAXLEN+1);
                  if (read) {
                      DEBUGP(DDEBUG, "autosync", "got %u bytes", read);
                      protoChoice[read] = '\0';
                      DEBUGP (DINFO,"autosync","client picked [%s]",protoChoice);	
                  } else {
                      DEBUGP (DERR,"autosync","unable to retrieve client's protocol choice!");
                  }
              } else {
                  DEBUGP (DERR,"autosync","unable to send supported protocols!");
              }
              break;
				
          case SESSION_CLIENT: {
              char *protocolsCopy = strdup (protocols);
              int idx = 0, slen = 0, clen = strlen(protocolsCopy);
              char *recvProtocols = NULL;
			
              if ((slen = I (Transport)->recv (session->transport, &recvProtocols, PROTOCOL_NAME_MAXLEN * AUTOSYNC_PROTOCOL_LIST_MAXLEN))) {
                  char *cp, *sp, *match = NULL;
                  for(cp = protocolsCopy; cp <= protocolsCopy+clen; cp++) {
                      if(cp != protocolsCopy && *(cp-1)!=':')
                          continue;
                          
                      for(sp = recvProtocols, idx = 0; sp <= (char *)(recvProtocols+slen); sp++) {
                          if(match && (*sp == '\0' ||  *sp == ':') && (*(cp+idx) == '\0' ||  *(cp+idx) == ':')) {
    
                              //*(cp+idx) = '\0';
                              // got a match!
                              free (recvProtocols);
						
                              char* tmpProto = malloc((idx+1) * sizeof(char));
                              memcpy(tmpProto, match, idx);
                              tmpProto[idx] = '\0';
						
                              DEBUGP (DINFO,"autosync","requesting compatible protocol [%s]", tmpProto);
														
                              if (I (Transport)->send (session->transport, tmpProto, idx)) {
                                  protoChoice = tmpProto;
                                  free (protocolsCopy);
                                  return protoChoice;
                              } else {
                                  DEBUGP (DERR,"autosync","unable to transmit protocol choice to server!");
                                  free (protocolsCopy);
                                  free (tmpProto);
                                  return NULL;
                              }
                          }

                          if(!match && sp != recvProtocols && (*(sp-1)!=':' || *sp != *(cp+idx)))
                              continue;
    
                          if(*sp != *(cp+idx)) {
                              idx = 0;
                              match = NULL;
                              continue;
                          }
    
                          match = cp;
                          idx++;
                      }
                  }
                  DEBUGP (DERR,"autosync","none of the protocols from server compatible with client's protocols [%s]",protocols);
                  free (recvProtocols);
				    
              } else {
                  DEBUGP (DERR,"autosync","unable to receive supported protocols from server!");
              }
              break;
          }
        }
    } else {
        DEBUGP (DERR,"autosync","cannot synchronize w/o session and list of protocols!");
    }
	return protoChoice;
}

static boolean_t
_sendMessage (session_t *session, message_t *message) {
	if (session && message) {
		if (session->state == SESSION_ACTIVE)
			return I (MessageQueue)->put (session->outgoingMessages, message);
	}
	return FALSE;
}

static message_t *
_recvMessage (session_t *session) {
	if (session) {
	    return I (MessageQueue)->get (session->incomingMessages);
	}
	return NULL;
}

/* use by Protocol layer to push or pop messages to Session layer */
static boolean_t
_pushMessage (session_t *session, message_t *message) {

	if (session && message) {
		return I (MessageQueue)->put (session->incomingMessages, message);
	}
		
	return FALSE;
}

static message_t *
_popMessage (session_t *session) {

	if (session) {
		return I (MessageQueue)->get (session->outgoingMessages);
	}
	return NULL;
}

IMPLEMENT_INTERFACE (Session) = {
	.new          = newSession,
	.destroy      = destroySession,
	.autosync     = _autosync,
	.sendMessage  = _sendMessage,
	.recvMessage  = _recvMessage,
	.pushMessage  = _pushMessage,
	.popMessage   = _popMessage
};

/***** SessionList Interface Implementation *****/

static inline session_table_t *
newSessionTable (void) {
	return (session_table_t *) I (List)->new ();
}

static inline session_entry_t *
firstSessionEntry (session_table_t *table) {
	return (session_entry_t *) I (List)->first ((list_t *) table);
}

static inline session_entry_t *
nextSessionEntry (session_entry_t *entry) {
	return I (List)->next (entry);
}

static inline session_entry_t *
prevSessionEntry (session_entry_t *entry) {
	return I (List)->prev (entry);
}

static inline session_entry_t *
lastSessionEntry (session_table_t *table) {
	return I (List)->last (table);
}

static uint32_t
countSessionTable (session_table_t *table) {
	return I (List)->count (table);
}

static boolean_t
addSessionEntry (session_table_t *table, session_t *session) {
	if (table && session) {
		return I (List)->insert (table, I (ListItem)->new (session))?TRUE:FALSE;
	}
	return FALSE;
}

static session_t *
getSessionFromEntry (session_entry_t *entry) {
	return (session_t *) entry->data;
}

static uint32_t
cleanSessionTable (session_table_t *table) {
	uint32_t cleanCounter = 0;
	list_item_t *item = I (List)->first (table);
	while (item) {
		list_item_t *next = I (List)->next (item);
		session_t *session = (session_t *)item->data;
		if (session->state == SESSION_TERMINATED) {
			list_item_t *old = I (List)->remove (table,item);
			DEBUGP (DDEBUG,"cleanSessionTable","getting rid of inactive session...");
			I (Session)->destroy (&session);
			I (ListItem)->destroy (&old);
			cleanCounter++;
		}
		item = next;
	}
	if (cleanCounter) {
		DEBUGP (DINFO,"cleanSessionTable","cleaned out %lu sessions!",(unsigned long)cleanCounter);
	}
	return cleanCounter;
}

static void
destroySessionTable (session_table_t **tablePtr) {
	/* get rid of all sessions! */
	if (tablePtr) {
		session_table_t *table = *tablePtr;
		if (table) {
			list_item_t *item = NULL;
			while ((item = I (List)->pop (table))) {
				session_t *session = I (SessionTable)->getSession (item);
				I (Session)->destroy (&session);
				I (ListItem)->destroy (&item);
			}
//			free (table);
//			*tablePtr = NULL;
			I (List)->destroy((list_t**)tablePtr);
		}
	}
}

IMPLEMENT_INTERFACE (SessionTable) = {
	.new        = newSessionTable,
	.first      = firstSessionEntry,
	.next       = nextSessionEntry,
	.prev       = prevSessionEntry,
	.last       = lastSessionEntry,
	.add        = addSessionEntry,
	.count      = countSessionTable,
	.getSession = getSessionFromEntry,
	.clean      = cleanSessionTable,
	.destroy    = destroySessionTable
};

/*//////// Transformation Interface Implementation //////////////////////////////////////////*/

TRANSFORM_EXEC (transport2sessionserver) {
    int sessin  = I (Parameters)->getByteValue (xform->blueprint,"session_incoming_queue_size");
    int sessout = I (Parameters)->getByteValue (xform->blueprint,"session_outgoing_queue_size");
    session_t *session = I (Session)->new (SESSION_SERVER, (transport_t *)in->data,sessin,sessout);
    if (session) {
        transform_object_t *obj = I (TransformObject)->new ("net:session:server",session);
        if (obj) {

	    char *paramRecords = I (Parameters)->getValue (xform->blueprint,"ssl_records");
            I (Transport)->useRecords (session->transport, paramRecords != NULL && *paramRecords == '1' ? 1 : 0);

            obj->destroy = (XDESTROY) I (Session)->destroy;
            return obj;
        }
        I (Session)->destroy (&session);
    }
    return NULL;
}

TRANSFORM_EXEC (transport2sessionclient) {
    int sessin  = I (Parameters)->getByteValue (xform->blueprint,"session_incoming_queue_size");
    int sessout = I (Parameters)->getByteValue (xform->blueprint,"session_outgoing_queue_size");
    session_t *session = I (Session)->new (SESSION_CLIENT, (transport_t *)in->data,sessin,sessout);
    if (session) {
        transform_object_t *obj = I (TransformObject)->new ("net:session:client",session);
        if (obj) {
            I (Transport)->useRecords (session->transport, TRUE); /* sessions ALWAYS uses records! */
            obj->destroy = (XDESTROY) I (Session)->destroy;
            return obj;
        }
        I (Session)->destroy (&session);
    }
    return NULL;
}

TRANSFORM_EXEC (session2sessionprotocol) {
    session_t *session = (session_t *)in->data;

    if (session) {
        char *protocols = I (Parameters)->getValue (xform->blueprint,"protocols");
        int syncTimeout = I (Parameters)->getTimeValue (xform->blueprint,"autosync_timeout");
        int sessTimeout = I (Parameters)->getTimeValue (xform->blueprint,"session_timeout");
        
        if (syncTimeout != -1)
            I (Transport)->setTimeout (session->transport, syncTimeout/1000);

        if (sessTimeout == -1)
            sessTimeout = DEFAULT_SESSION_PROTOCOL_TIMEOUT;
        else
            sessTimeout /= 1000;
            
        if (protocols) {
            char *protocolChoice = I (Session)->autosync (session, protocols);
            if (protocolChoice) {
                char *protoformat = I (String)->copy ("net:session:protocol::");
                I (String)->join (&protoformat,protocolChoice);
                free (protocolChoice);
                
                DEBUGP (DINFO,"session2sessionprotocol","autosync to use '%s' protocol format", protoformat);
                
                session->protocol = protoformat;
        
                I (Transport)->setTimeout (session->transport, sessTimeout);

                transform_object_t *obj = I (TransformObject)->new (session->protocol,session);
                if (obj) {
                    I (TransformObject)->save (obj); /* save since copy of session */
                    return obj;
                }
            } else {
                DEBUGP (DERR,"session2sessionprotocol","unable to autosync protocol selection for this session!");
            }
        } else {
            DEBUGP (DERR,"session2sessionprotocol","cannot perform this transform without list of supported protocols provided!");
        }
    }
    return NULL;
}

TRANSFORM_NEW (newNetSessionTransformation) {

    TRANSFORM ("net:transport","net:session::server",transport2sessionserver);
    TRANSFORM ("net:transport","net:session::client",transport2sessionclient);
    TRANSFORM ("net:session::*","net:session:protocol::*",session2sessionprotocol);
    
    IF_TRANSFORM (session2sessionprotocol) {
        /* verify that parameters are provided */
        TRANSFORM_HAS_PARAM ("protocols");
    }
    
} TRANSFORM_NEW_FINALIZE;

TRANSFORM_DESTROY (destroyNetSessionTransformation) {

} TRANSFORM_DESTROY_FINALIZE;


IMPLEMENT_INTERFACE (Transformation) = {
	.new       = newNetSessionTransformation,
	.destroy   = destroyNetSessionTransformation,
	.execute   = NULL,
	.free      = NULL
};

