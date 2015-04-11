#include <corenova/source-stub.h>

THIS = {
	.version     = "0.9",
	.author      = "Hash Yuan <hyuan@clearpathnet.com>",
	.description = "a remote procedure call module",
	.requires    = LIST ("corenova.data.array",
			"corenova.net.tcp",
			"corenova.sys.transform",
			"corenova.data.pbc"),
	.implements  = LIST ("RpcServices")
};

#include <netinet/in.h>
#include <corenova/net/socket.h>
#include <corenova/net/resolve.h>
#include <corenova/net/tcp.h>
#include <corenova/net/rpc.h>

static array_t *rpcs;

static void registry(transformation_t *xform)
{
	if(rpcs == NULL) rpcs = I (Array)->new();
	I (Array)->add(rpcs, xform);
}

static inline transformation_t *lookup(char *from, char *to)
{
	transformation_t *xform = NULL;
	int i = 0;

	DEBUGP (DINFO, "RPC lookup", "lookup from %s, to %s\n", from, to);	
	while ((xform = I (Array)->get(rpcs, i ++)))
	{
		if (strcmp(xform->from, from) == 0 && strcmp(xform->to, to) == 0)
			return xform;
	}
	
	return NULL;
}

/* Remote Procedure Call */
static transform_object_t * request (transformation_t *xform, transform_object_t *in)
{
	if (xform->rpcops) {
		transform_object_t *result = NULL;
		tcp_t *tcp = I(TcpConnector)->connect(xform->to, RPC_PORT);
		if (tcp) {
			int size;
			unsigned char *pack;
			
			
			size = I(PbcOps)->req_pack(xform->from, xform->to, in->data, xform->rpcops, &pack);
			if (size) {
				size = I(TcpConnector)->write (tcp, (char *)pack, size);
				if (size) {
					char buf[1024];
					char *p = (char *)buf;
					
					size = I(TcpConnector)->read(tcp, &p, sizeof(buf));
					if (size) {
						Rpc__Response *res = I(PbcOps)->rpc_res_unpack(size, buf);
						if (res) {
							/* read the code */
							if (res->code == RPC_SUCCESS) {
								result = I(PbcOps)->res_unpack(res, xform->rpcops);
							} else {
								DEBUGP (DINFO, "RPC request", "response with err code %d\n", res->code);
							}
							
							I(PbcOps)->res_free(res);
						} else {
							DEBUGP (DERROR, "RPC request", "rpc_res_unpack failed\n");
						}
					} else {
						DEBUGP (DERROR, "RPC request", "don't get response from %s, RPC_TIMEOUT\n", xform->to);
					}
				} else {
					DEBUGP (DERROR, "RPC request", "cant write to %s\n", xform->to);
				}
				
				free(pack);
			} else {
				DEBUGP (DERROR, "RPC request", "can't pack\n");
			}
			
			I(TcpConnector)->destroy(&tcp);
		} else {
			DEBUGP (DERROR, "RPC request", "can't connect to host %s\n", xform->to);
		}
		
		if (result) {
			return result;
		} else {
			/* sometimes need to return something */
			Rpc__Response err;
			memset(&err, 0, sizeof(Rpc__Response));
	
			err.format = xform->to;
			return I(PbcOps)->res_unpack(&err, xform->rpcops);
		}
	} else {
		DEBUGP (DERROR, "RPC request", "no rpcops for %s -> %s\n", xform->from, xform->to);
 	}
 	
 	return NULL;
}

/* parse RPC request data, generate RPC response data */
static int reply(char *in, size_t size, char **out)
{
	int code = RPC_UNKNOWN_ERR;
	transformation_t *xform = NULL;
	unsigned char *pack = NULL;

	Rpc__Request *req = I(PbcOps)->rpc_req_unpack(size, in);
	if (req) {
		xform = lookup(req->from, req->to);
		if (xform) {
			transform_object_t *in, *out;
		
			if (req->data.data && req->data.len > 0
				&& (in = I(PbcOps)->req_unpack(req, xform->rpcops))) {
			
				out = (*xform->exec)(xform, in);
				if (out) {
					size = I(PbcOps)->res_pack(out->format, out->data, xform->rpcops, &pack);
					code = RPC_SUCCESS;

					I (TransformObject)->destroy(&out);
				} else {
					code = RPC_REMOTE_EXEC_FAIL;
				}
			
				I (TransformObject)->destroy(&in);
			} else {
				code = RPC_PARAMETER_ERR;
			}
		} else {
			code = RPC_SERVICE_NOT_SUPPORTED;
		}
		
		I(PbcOps)->req_free(req);
	} else {
		DEBUGP (DINFO, "RPC reply", "rpc_req_unpack failed \n");
	}

	/* pack code and send data */
	size = I(PbcOps)->code_pack(&pack, size, code);
	*out = pack;

	return size;
}

IMPLEMENT_INTERFACE (RpcServices) = {
	.registry	= registry,
	.lookup		= lookup,
	.request	= request,
	.reply		= reply
};
