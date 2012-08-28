#include <corenova/source-stub.h>

THIS = {
	.version     = "2.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description =
	"This module provides a mechanism for tracking TCP sessions when doing traffic analysis.",
	.implements  = LIST ("TCPTracker"),
	.requires    = LIST ("corenova.data.configuration",
						 "corenova.data.list",
						 "corenova.data.message",
						 "corenova.data.queue",
						 "corenova.net.protocol",
						 "corenova.net.transport")
};

#include <corenova/net/tcptracker.h>

/*//////// MODULE CODE //////////////////////////////////////////*/


static inline tcpsession_table_t *
newTCPSessionTable (uint32_t timeOut) {

	tcpsession_table_t *table = calloc(1, sizeof(tcpsession_table_t *));
	
	table->session_table = I (List)->new ();
	table->timeOut = timeOut;
	
	MUTEX_SETUP(table->lock);

	return table;
	
}

static tcpsession_entry_t *
addTCPSession(tcpsession_table_t *table, in_addr_t *src_addr, in_addr_t *dst_addr, in_port_t src_port, in_port_t dst_port) {

    tcpsession_entry_t *session = calloc(1, sizeof(tcpsession_entry_t));

    if (table && session) {
    
	// blah blah blah
    
	
	return (tcpsession_entry_t *)(I (List)->insert (table->session_table, I (ListItem)->new (session)))->data;

    }

    return NULL;
    
}

static void
removeTCPSession(tcpsession_table_t *table, in_addr_t *src_addr, in_addr_t *dst_addr, in_port_t src_port, in_port_t dst_port) {
}

static tcpsession_entry_t *
findTCPSession(tcpsession_table_t *table, in_addr_t *src_addr, in_addr_t *dst_addr, in_port_t src_port, in_port_t dst_port) {

    return NULL;

}

static void
clearTCPSessionTable (tcpsession_table_t *table) {

    MUTEX_LOCK(table->lock);
    
    list_item_t *n; 
    
    while((n = I (List)->pop(table->session_table))!=NULL) {
	
	free(n->data);
	I (ListItem)->destroy(&n);
	
    }

    MUTEX_UNLOCK(table->lock);

}

static void
destroyTCPSessionTable (tcpsession_table_t **tablePtr) {

    if(tablePtr && *tablePtr) {
    
	I (List)->destroy(&((*tablePtr)->session_table));
	
	MUTEX_CLEANUP((*tablePtr)->lock);
	
	free(*tablePtr);
	
	tablePtr = NULL;    
    
    }

}

IMPLEMENT_INTERFACE (TCPSessionTable) = {
	.new        = newTCPSessionTable,
	.add        = addTCPSession,
	.remove     = removeTCPSession,
	.find       = findTCPSession,
	.clear      = clearTCPSessionTable,
	.destroy    = destroyTCPSessionTable
};
