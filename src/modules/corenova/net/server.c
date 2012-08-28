#include <corenova/source-stub.h>

THIS = {
	.version     = "3.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "This is a module that provides network server operations",
	.implements  = LIST ("NetServer","Transformation"),
	.requires    = LIST ("corenova.data.parameters",
                         "corenova.data.array",
						 "corenova.data.string",
                         "corenova.sys.transform",
                         "corenova.net.transport"),
    .transforms = LIST ("transform:feeder -> net:server",
                        "net:server -> net:server::tcp",
                        "net:server::tcp -> net:server::ssl",
                        "net:server::tcp -> net:transport",
                        "net:server::ssl -> net:transport")
};

#include <corenova/net/server.h>
#include <corenova/data/array.h>
#include <corenova/data/string.h>
#include <corenova/sys/transform.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

static net_server_t *
newNetworkServer (parameters_t *conf) {
    net_server_t *instance = (net_server_t *)calloc (1,sizeof (net_server_t));
    if (instance) {
        if (conf) {
            if (I (Parameters)->getValue (conf,"listen_port")) {
                instance->listen_port = atoi (I (Parameters)->getValue (conf,"listen_port"));
                if (!instance->listen_port) {
                    DEBUGP (DERR,"newNetworkServer","invalid 'listen_port' value '%s'",I (Parameters)->getValue (conf,"listen_port"));
                    free (instance);
                    return NULL;
                }
            } else {
                DEBUGP (DERR,"newNetworkServer", "must provide 'listen_port' as parameter!");
                free (instance);
                return NULL;
            }
        } else {
            free (instance);
            return NULL;
        }
    }
    return instance;
}

static boolean_t
listenNetworkServer (net_server_t *instance) {
    if (instance) {
        if (!instance->listenSocket) {
            DEBUGP (DINFO,"listenNetworkServer","trying to listen on port:%d...",instance->listen_port);
            if (instance->use_ssl) {
                instance->listenSocket = I (SSLConnector)->listen (instance->listen_port);
            } else {
                instance->listenSocket = I (TcpConnector)->listen (instance->listen_port);
            }
            
            if (instance->listenSocket) {
                DEBUGP (DINFO,"listenNetworkServer","successfully opened listening socket on port:%d!",instance->listen_port);
            }
        }
        
        if (instance->listenSocket) {
            return TRUE;
        }
    }
    return FALSE;
}

static transport_t *
acceptNetworkServer (net_server_t *instance) {
    transport_t *transport = NULL;
    if (instance && instance->listenSocket) {
        DEBUGP (DINFO,"acceptNetworkServer","blocking on accept to retrieve the transport connection!");
        if (instance->use_ssl) {
            tcp_t *tcpConnection = I (TcpConnector)->accept (instance->listenSocket);
            ssl_t *sslConnection = I (SSLConnector)->accept (instance->ctx,tcpConnection);
            transport = I (Transport)->new (TRANSPORT_SSL,sslConnection);
        } else {
            tcp_t *tcpConnection = I (TcpConnector)->accept (instance->listenSocket);
            I (TcpConnector)->useRecords (tcpConnection,FALSE);
            transport = I (Transport)->new (TRANSPORT_TCP,tcpConnection);
        }
    }
    return transport;
}

static void
destroyNetworkServer (net_server_t **ptr) {
    if (ptr) {
        net_server_t *instance = *ptr;
        if (instance) {
            if (instance->use_ssl) {
                I (SSLConnector)->destroyContext (&instance->ctx);
            }
            I (Socket)->destroy (&instance->listenSocket);
            free (instance);
            *ptr = NULL;
        }
    }
}

IMPLEMENT_INTERFACE (NetServer) = {
    .new     = newNetworkServer,
    .listen  = listenNetworkServer,
    .accept  = acceptNetworkServer,
    .destroy = destroyNetworkServer
};

/*//////// Transformation Interface Implementation //////////////////////////////////////////*/

TRANSFORM_EXEC (feeder2server) {
    socket_t *socket = (socket_t *)xform->instance;
    
    
    if (!socket) {
        int port = atoi (I (Parameters)->getValue (xform->blueprint,"listen_port"));
        if (port) {
            socket = xform->instance = I (TcpConnector)->listen (port);
        }
    }

    transform_object_t *obj = I (TransformObject)->new ("net:server",socket);
    if (obj) I (TransformObject)->save (obj);
    return obj;
}

TRANSFORM_EXEC (server2tcp) {
    socket_t *server = (socket_t *)in->data;
    if (server) {
    
        tcp_t *tcp = I (TcpConnector)->accept (server);
        transform_object_t *obj = I (TransformObject)->new (xform->to, tcp);
        if (obj) {
            obj->destroy = (XDESTROY) I (TcpConnector)->destroy;
            return obj;
        }
    }
    return NULL;
}

TRANSFORM_EXEC (tcp2ssl) {
    tcp_t *tcp = (tcp_t *)in->data;
    ssl_context_t *ctx = (ssl_context_t *)xform->instance;
    if (tcp && ctx) {

        ssl_t *ssl = I (SSLConnector)->accept (ctx,tcp);
        
        char *paramRecords = I (Parameters)->getValue (xform->blueprint,"ssl_records");
        
	DEBUGP(DERROR, "tcp2ssl", "got new ssl connection");

        if (ssl) {
        
            I (SSLConnector)->useRecords(ssl, paramRecords != NULL && *paramRecords == '1' ? 1 : 0);

            transform_object_t *obj = I (TransformObject)->new (xform->to, ssl);
            if (obj) {
                obj->destroy = (XDESTROY) I (SSLConnector)->destroy;
                return obj;
            }
        }
        DEBUGP (DERR,"tcp2ssl","unable to perform SSL accept on passed in TCP connection from: %s:%u",tcp->srcHostName, tcp->srcHostPort);
    }
    return NULL;
}

TRANSFORM_EXEC (tcp2transport) {
    tcp_t *tcp = (tcp_t *)in->data;
    if (tcp) {
        // no need to specify XDESTROY because transport object does not have any dynamic memory components
        return I (TransformObject)->new (xform->to,I (Transport)->new (TRANSPORT_TCP,tcp));
    }
    return NULL;
}

TRANSFORM_EXEC (ssl2transport) {
    ssl_t *ssl = (ssl_t *)in->data;
    if (ssl) {
        // no need to specify XDESTROY because transport object does not have any dynamic memory components
        return I (TransformObject)->new (xform->to,I (Transport)->new (TRANSPORT_SSL,ssl));
    }
    return NULL;
}

TRANSFORM_NEW (newNetServerTransformation) {

    TRANSFORM ("transform:feeder","net:server",feeder2server);
    TRANSFORM ("net:server","net:server::tcp",server2tcp);
    TRANSFORM ("net:server::tcp","net:server::ssl",tcp2ssl);
    TRANSFORM ("net:server::tcp","net:transport",tcp2transport);
    TRANSFORM ("net:server::ssl","net:transport",ssl2transport);
    
    IF_TRANSFORM (feeder2server) {
        // for now, assume we only handle TCP connections
        TRANSFORM_HAS_PARAM ("listen_port");
    }
    
    IF_TRANSFORM (tcp2ssl) {
        TRANSFORM_WITH (
            I (SSLConnector)->context (SSL_SERVER,
                                       I (Parameters)->getValue (blueprint,"ssl_certfile"),
                                       I (Parameters)->getValue (blueprint,"ssl_keyfile"),
                                       I (Parameters)->getValue (blueprint,"ssl_password"),
                                       I (Parameters)->getValue (blueprint,"ssl_ca_list"),
                                       I (Parameters)->getValue (blueprint,"ssl_ciphers"),
        			       I (Parameters)->getValue (blueprint,"client_auth"),
                                       I (Parameters)->getValue (blueprint,"ssl_dhfile")));
    }
    
} TRANSFORM_NEW_FINALIZE;

TRANSFORM_DESTROY (destroyNetServerTransformation) {

    IF_TRANSFORM (feeder2server) {
        socket_t *instance = (socket_t *)xform->instance;
        I (Socket)->destroy (&instance);
    }

    IF_TRANSFORM (tcp2ssl) {
        ssl_context_t *ctx = (ssl_context_t *)xform->instance;
        I (SSLConnector)->destroyContext (&ctx);
    }
    
} TRANSFORM_DESTROY_FINALIZE;

IMPLEMENT_INTERFACE (Transformation) = {
	.new       = newNetServerTransformation,
	.destroy   = destroyNetServerTransformation,
	.execute   = NULL,
	.free      = NULL
};

