#include <corenova/source-stub.h>

THIS = {
	.version     = "2.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "This module provides loading & unloading of protocols.",
	.implements  = LIST ("ProtocolLoader"),
	.requires    = LIST ("corenova.sys.loader","corenova.data.string")
};

#include <corenova/net/protocol.h>
#include <corenova/data/string.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

static protocol_t *
loadProtocolModule (const char *protocol, const char *protocolPrefix) {
    protocol_t *proto = NULL;
    if (protocol && protocolPrefix) {
        list_t *prefixes = I (String)->tokenize (protocolPrefix,":");
        if (prefixes) {
            list_item_t *item = NULL;
            while ((item = I (List)->pop (prefixes))) {
                char *prefix = (char *)item->data;
                if (prefix) {
                    char *protomodule = I (String)->new ("%s.%s",prefix,protocol);
                    if (protomodule) {										 
                        proto = I (DynamicLoader)->load (strdup(protomodule));
                        free (protomodule);
                    }
                    free (prefix);
                }
                I (ListItem)->destroy (&item);
                if (proto) break;
            }
            I (List)->clear (prefixes,TRUE);
            I (List)->destroy (&prefixes);
        }
    }
	return proto;
}

static void 
unloadProtocolModule (protocol_t *protocol) {
	I (DynamicLoader)->unload (protocol);
}

IMPLEMENT_INTERFACE (ProtocolLoader) = {
	.load   = loadProtocolModule,
	.unload = unloadProtocolModule
};
