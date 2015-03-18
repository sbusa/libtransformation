#ifndef __net_rpc_H__
#define __net_rpc_H__

#include <corenova/interface.h>

#include <corenova/sys/transform.h>
#include <corenova/data/pbc.h>

typedef struct {
	transformation_t *xform;
	void *data;
} rpc_service_t;

/* the data is presented as protobuf format
 * 	request				       reply
 *		|------------|			|-----------|
 *		|   magic#   |          | err code  |
 *      |------------|          |-----------|
 *  	|    from    |          |    to     |
 *		|------------|          |-----------|
 *		|     to     |          |  result   |
 *		|------------|          |-----------|
 *		|            |
 *		|  in->data  |
 *      |            |
 *      |------------|
 */
#define MAGIC	0x43454E41	//"CENA"
#define RPC_PORT 12345

typedef enum {
	RPC_SUCCESS = 1,
	RPC_CONNECT_FAIL,
	RPC_SERVICE_NOT_SUPPORTED,
	RPC_PARAMETER_ERR,
	RPC_REMOTE_EXEC_FAIL,
	RPC_TIMEOUT,
	RPC_UNKNOWN_ERR
} rpc_err_t;

DEFINE_INTERFACE (RpcServices) {
	void (*registry) (transformation_t *xform);
	transformation_t *(*lookup) (char *from, char *to);
	transform_object_t *(*request) (transformation_t *xform, transform_object_t *in);
	int (*reply) (char *in, size_t size, char **out);
};
#endif
