#include <corenova/source-stub.h>

THIS = {
	.version     = "3.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "This is a module that provides network filtering operations",
	.implements  = LIST ("NetFilter","Transformation"),
	.requires    = LIST ("corenova.data.parameters",
                         "corenova.data.array",
						 "corenova.data.string",
                         "corenova.sys.transform",
                         "corenova.net.transport"),
    .transforms = LIST ("net:transport -> net:filter:access",
                        "net:filter:access -> net:transport",
                        "net:filter:access -> data:log:message",
                        "net:proxy:session:* -> net:filter:protocol:*",
                        "net:filter:protocol:* -> net:proxy:session:*")
};

#include <corenova/net/filter.h>
#include <corenova/data/array.h>
#include <corenova/data/string.h>
#include <corenova/sys/transform.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

static net_filter_t *
newNetworkFilter (parameters_t *conf) {
    if (conf) {
        net_filter_t *instance = (net_filter_t *)calloc (1,sizeof (net_filter_t));
        if (instance) {
            char *access_acl = I (Parameters)->getValue (conf,"access_acl");
            char *protocol_acl  = I (Parameters)->getValue (conf,"protocol_acl");

            if (access_acl) {
                
            }

            if (protocol_acl) {
                
            }
            return instance;
        }
    }
    return NULL;
}

static net_filter_access_t *
filterAccessNetworkFilter (net_filter_t *instance, transport_t *transport) {
    if (instance && transport) {
        net_filter_access_t *access = (net_filter_access_t *)calloc (1,sizeof (net_filter_access_t));
        if (access) {
            // XXX - TBI
            access->allow = TRUE;
            return access;
        }
    }
    return NULL;
}

/* 
 * static net_filter_access_t *
 * filterProtocolNetworkFilter (net_filter_t *instance, net_analysis_t *analysis) {
 *     if (instance && transport) {
 *         net_filter_access_t *access = (net_filter_access_t *)calloc (1,sizeof (net_filter_access_t));
 *         if (access) {
 *             // XXX - TBI
 *             access->allow = TRUE;
 *             return access;
 *         }
 *     }
 *     return NULL;
 * }
 */

static void
destroyNetworkFilter (net_filter_t **ptr) {
    if (ptr) {
        net_filter_t *instance = *ptr;
        if (instance) {
            free (instance);
            *ptr = NULL;
        }
    }
}

IMPLEMENT_INTERFACE (NetFilter) = {
    .new            = newNetworkFilter,
    .filterAccess   = filterAccessNetworkFilter,
//    .filterProtocol = filterProtocolNetworkFilter,
//    .filterProtocol = NULL,
    .destroy        = destroyNetworkFilter
};

/*//////// Transformation Interface Implementation //////////////////////////////////////////*/

TRANSFORM_EXEC (transport2filter) {
    net_filter_t *instance = (net_filter_t *)xform->instance;
    transport_t *transport = (transport_t *)in->data;
    net_filter_access_t *access = I (NetFilter)->filterAccess (instance, transport);
    if (access) {
        transform_object_t *obj = I (TransformObject)->new ("net:filter:access",access);
        if (obj) {
            obj->destroy = (XDESTROY) I (NetFilter)->destroy;
            return obj;
        }
        free (access);
    }
    return NULL;
}

TRANSFORM_EXEC (filter2transport) {
    net_filter_access_t *access = (net_filter_access_t *)in->data;
    if (access && access->allow) {
        if (in->originator && I (String)->equal (in->originator->format,"net:transport")) {
            transform_object_t *obj = I (TransformObject)->new ("net:transport",(transport_t *) in->originator->data);
            if (obj) {
                I (TransformObject)->save (obj);
                return obj;
            }
        }
    }
    return NULL;
}

TRANSFORM_EXEC (analysis2filter) {
    return NULL;
}

TRANSFORM_EXEC (filter2analysis) {
    return NULL;
}

TRANSFORM_EXEC (protocol2filter) {
    return NULL;
}

TRANSFORM_EXEC (filter2protocol) {
    return NULL;
}

TRANSFORM_NEW (newNetFilterTransformation) {

    TRANSFORM ("net:transport", "net:filter:access", transport2filter);
    TRANSFORM ("net:filter:access", "net:transport", filter2transport);

    TRANSFORM ("net:analysis", "net:filter:access", analysis2filter);
    TRANSFORM ("net:filter:access", "net:analysis", filter2analysis);

    TRANSFORM ("net:protocol", "net:filter:access", protocol2filter);
    TRANSFORM ("net:filter:access", "net:protocol", filter2protocol);

    IF_TRANSFORM (transport2filter) {
        TRANSFORM_WITH (I (NetFilter)->new (blueprint));
    }

    IF_TRANSFORM (analysis2filter) {
        TRANSFORM_WITH (I (NetFilter)->new (blueprint));
    }

    IF_TRANSFORM (protocol2filter) {
        TRANSFORM_WITH (I (NetFilter)->new (blueprint));
    }

} TRANSFORM_NEW_FINALIZE;

TRANSFORM_DESTROY (destroyNetFilterTransformation) {

    IF_TRANSFORM (transport2filter) {
        net_filter_t *filter = (net_filter_t *)xform->instance;
        I (NetFilter)->destroy (&filter);
    }

    IF_TRANSFORM (analysis2filter) {
        net_filter_t *filter = (net_filter_t *)xform->instance;
        I (NetFilter)->destroy (&filter);
    }

    IF_TRANSFORM (protocol2filter) {
        net_filter_t *filter = (net_filter_t *)xform->instance;
        I (NetFilter)->destroy (&filter);
    }

} TRANSFORM_DESTROY_FINALIZE;


IMPLEMENT_INTERFACE (Transformation) = {
	.new       = newNetFilterTransformation,
	.destroy   = destroyNetFilterTransformation,
	.execute   = NULL,
	.free      = NULL
};

