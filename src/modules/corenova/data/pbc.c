#include <corenova/source-stub.h>

THIS = {
	.version     = "0.9",
	.author      = "Hash Yuan <hyuan@clearpathnet.com>",
	.description = "a protocol buffer C module",
	.implements  = LIST ("PbcOps")
};


#include <corenova/data/pbc.h>
#include "parser/corenova.pb-c.c"

/* the define changed along with corenova.proto */
int req_pack(char *from, char *to, char *data, void *ops, unsigned char **out)
{
	size_t size;
	unsigned char *pack = NULL;
	Rpc__Request req = RPC__REQUEST__INIT;
	
	req.from = from;
	req.to = to;
	req.data.len = (*((_pbc_operation_t *)ops)->_req_pack)(data, &req.data.data);
	if (req.data.len) {
		size = rpc__request__get_packed_size(&req);
		if (size) pack = malloc (size);
		if (pack) {
			if (size == rpc__request__pack(&req, pack)) {
				*out = pack;
				return size;
			}
			
			free(pack);
		}
	}
	
	return 0;
}

Rpc__Request *rpc_req_unpack(size_t size, char *in)
{
	if (size > 0 && in)
		return rpc__request__unpack(NULL, size, (unsigned char *)in); 

	return NULL;	
}

void *req_unpack(Rpc__Request *req, void *ops) 
{
	if (req && ops) {
		return (*((_pbc_operation_t *)ops)->_req_unpack)(req->from, req->data.data, req->data.len);
	}
	
	return NULL;
}

int res_pack(char *format, char *data, void *ops, unsigned char **out) 
{
	size_t size;
	unsigned char *pack = NULL;
	Rpc__Response res = RPC__RESPONSE__INIT;

	res.code = 0xf;	
	res.format = format;
	res.data.len = (*((_pbc_operation_t *)ops)->_res_pack)(data, &res.data.data);
	if (res.data.len) res.has_data = 1;	
	
	size = rpc__response__get_packed_size(&res);
	if (size) pack = malloc (size);
	if (pack) {
		if (size == rpc__response__pack(&res, pack)) {
			*out = pack;
			return size;
		}

		free(pack);
	}

	return 0;
}

Rpc__Response *rpc_res_unpack(size_t size, char *in)
{
        if (size > 0 && in)
                return rpc__response__unpack(NULL, size, (unsigned char *)in);

        return NULL;
}


void *res_unpack(Rpc__Response *res, void *ops) 
{
	if (res) {
		return (*((_pbc_operation_t *)ops)->_res_unpack)(res->format, res->data.data, res->data.len);
	}

	return NULL;
}

/* Base 128 Varints */
char err_code[] = {0xd0, 0x00};
int code_pack(unsigned char **pack, size_t size, int code)
{
	if (size > 0 && pack && *pack) {
		(*pack)[1] = code;
		return size;
	} else {
		if (*pack) free (*pack); 
		*pack = malloc (sizeof (err_code));
		if (*pack) {
			memcpy(*pack, err_code, sizeof (err_code));
			(*pack)[1] = code;
			return sizeof (err_code);
		}
	}

	return 0;
}

int code_unpack (unsigned char *pack, size_t size)
{
	if (size < sizeof(err_code)) {
		return pack[1];
	} else {
		fprintf(stderr, "the response data is less\n");
	}
	
	return -1;
}

IMPLEMENT_INTERFACE (PbcOps) = {
	.req_pack = req_pack,
	.rpc_req_unpack = rpc_req_unpack,
	.req_unpack = req_unpack,
	.res_pack = res_pack,
	.rpc_res_unpack = rpc_res_unpack,
	.res_unpack = res_unpack,
	.code_pack = code_pack,
	.code_unpack = code_unpack
};
