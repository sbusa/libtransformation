#ifndef __data_pbc_H__
#define __data_pbc_H__

#include <corenova/interface.h>
#include <corenova/data/parser/corenova.pb-c.h>

DEFINE_INTERFACE (PbcOps) {
	int (*req_pack) (char *from, char *to, char *data, void *ops, unsigned char **out);
	Rpc__Request *(*rpc_req_unpack)(size_t size, char *in);
	void *(*req_unpack) (Rpc__Request *req, void *ops);
	int (*res_pack) (char *format, char *data, void *ops, unsigned char **out);
	Rpc__Response *(*rpc_res_unpack)(size_t size, char *in);
	void *(*res_unpack) (Rpc__Response *res, void *ops);
	int (*code_pack) (unsigned char **pack, size_t size, int code);
	int (*code_unpack) (unsigned char *pack, size_t size);
};

typedef struct _pbc_operation {
	size_t (*_req_pack)(void *in, unsigned char **out);
	void *(*_req_unpack)(char *format, unsigned char *data, size_t size);
	size_t (*_res_pack)(void *in, unsigned char **out);
	void *(*_res_unpack)(char *format, unsigned char *data, size_t size);
} _pbc_operation_t;

#endif
