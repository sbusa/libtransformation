#ifndef __protocol_H__
#define __protocol_H__

#include <corenova/interface.h>

#include <corenova/sys/loader.h>

#define PROTOCOL_NAME_MAXLEN 32	 /* maximum length of name string */

typedef module_t protocol_t;	/* protocol is dynamically loaded */

DEFINE_INTERFACE (ProtocolLoader) {
	/*
	  returns a PROTOCOL object (a newly dynamically loaded module)
	  provided the PROTOCOL name and the module prefix(es) for the
	  given name.

	  The protocol prefix is in the format of xxx.yyy.*:aaa.bbb.*

      example: corenova.net.proto:cpn.net.proto
	*/
	protocol_t *(*load)   (const char *protocol, const char *protocolPrefix);

	/*
	  unload the protocol object from the runtime memory.
	*/
	void        (*unload) (protocol_t *);
};

#endif
