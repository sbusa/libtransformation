#ifndef __session_H__
#define __session_H__

#include <corenova/interface.h>

#include <corenova/data/message.h>
#include <corenova/net/protocol.h>
#include <corenova/net/transport.h>
#include <corenova/sys/quark.h>

#define AUTOSYNC_PROTOCOL_LIST_MAXLEN 255 /* maximum list of protocols
										   * autosync supports */
#define DEFAULT_SESSION_PROTOCOL_TIMEOUT 600 /* 600 seconds */

#define DEFAULT_SESSION_QUEUE_MAXSIZE 1000 /* 1000 entries */


typedef struct {

	enum session_mode {
		SESSION_SERVER = 1,
		SESSION_CLIENT
	} mode;

	enum session_state {
		SESSION_ACTIVE = 1,
		SESSION_TERMINATED
	} state;

	transport_t     *transport;
    char            *protocol; /* chosen protocol to engage session */
	void            *userData; /* internal space for use by protocol */
	quark_t         *quark;    /* The spark of communication loop */

    char            *from;      /* a place to store cname? */
    
	/* Messaging Queues */
	message_queue_t *incomingMessages;
	message_queue_t *outgoingMessages;

	#define SESSION_CHUNK_SIZE 100
	#define SESSION_MAX_CHUNKS 100
	
} session_t;

typedef struct {

    char *format;
    session_t  *session;
    protocol_t *protocol;
    
} session_protocol_t;

#include <corenova/data/configuration.h>

DEFINE_INTERFACE (Session) {
	session_t  *(*new)          (enum session_mode, transport_t *transport, int insize, int outsize);
	void        (*destroy)      (session_t **);
	char       *(*autosync)     (session_t *, const char *protocols);
	boolean_t   (*sendMessage)  (session_t *, message_t *);
	message_t  *(*recvMessage)  (session_t *);

	/* for use by SessionProtocol implementaions */
	boolean_t   (*pushMessage)  (session_t *, message_t *);
	message_t  *(*popMessage)   (session_t *);
};

#include <corenova/data/list.h>

typedef list_t      session_table_t;
typedef list_item_t session_entry_t;

DEFINE_INTERFACE (SessionTable) {
	session_table_t *(*new)        (void);

	session_entry_t *(*first)      (session_table_t *);
	session_entry_t *(*next)       (session_entry_t *);
	session_entry_t *(*prev)       (session_entry_t *);
	session_entry_t *(*last)       (session_table_t *);

	uint32_t         (*count)      (session_table_t *);
	
	boolean_t        (*add)        (session_table_t *,session_t *);
	session_t       *(*getSession) (session_entry_t *);
	
	uint32_t         (*clean)      (session_table_t *);
	void             (*destroy)    (session_table_t **);
};

/*
  all Session-oriented PROTOCOL modules must implement this interface!
*/
DEFINE_INTERFACE (SessionProtocol) {
    session_protocol_t *(*new)     (session_t *, protocol_t *);
    void                (*destroy) (session_protocol_t **);

    /* XXX - actually the following only... conflict in interface name! BAH BAH BAH! */
	boolean_t (*start) (session_t *);
	void      (*stop)  (session_t *);
};

DEFINE_INTERFACE (SessionHack) {
	char *(*getPeerCname) (session_t *);
};

#endif
