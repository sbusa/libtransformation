#ifndef __ssl_H__
#define __ssl_H__

#include <corenova/interface.h>
#include <corenova/data/cache.h>

#include <corenova/net/tcp.h>

#if !defined (HAVE_LIBKERBEROS) && !defined (OPENSSL_NO_KRB5)
# define OPENSSL_NO_KRB5
#endif

#include <openssl/ssl.h>		/* for obvious reasons... */

const char SSL_default_ciphers[] =
	"EDH-RSA-DES-CBC3-SHA:"
	"EDH-DSS-DES-CBC3-SHA:"
	"DES-CBC3-SHA:"
	"DES-CBC3-MD5:"
	"DHE-DSS-RC4-SHA:"
	"RC4-SHA:RC4-MD5:"
	"RC2-CBC-MD5:"
	"RC4-MD5:"
	"RC4-64-MD5:"
	"EXP1024-DHE-DSS-RC4-SHA";

/**
 * TYPE DECLARATION
 * --------------------
 * List out defines and types that will be set for other modules that are
 * dependent on this module.
 **/
#define MAX_REC_SIZE       16384 /* (max positive int) */

#define SSL_SUCCESS   0
#define SSL_TRY_AGAIN 1
#define SSL_SHUTDOWN  2
#define SSL_RETURN_WITH_DATA 3

typedef enum {
	SSL_SERVER = 0,
	SSL_CLIENT = 1
} ssl_mode_t;

typedef SSL_CTX ssl_context_t;	/* alias the ssl context structure */

typedef struct {
	ssl_mode_t mode;			/* ssl mode SSL_CLIENT or SSL_SERVER */
	SSL_CTX *ctx;				/* ssl context */
	SSL *conn;					/* our ssl connection */
	BIO *sslBio;				/* BIO object to link our sock to SSL */
	BIO *errBio;				/* SSL BIO error context */
	SSL_SESSION *session;		/* existing session */

	tcp_t *tcp; 				/* the underlying tcp connection */

	/* the following is deprecated for now... */
	unsigned char nblock  :1; // 1 if nonblocking 0 else
	unsigned char r_bo_w  :1; // read blocked on write
	unsigned char w_bo_r  :1; // write blocked on read
	unsigned char r_block :1; // read blocked
	unsigned char records :1; // if using record headers
	unsigned char error   :3; // save error state
	
	unsigned char forcetcp :1; // use raw TCP
   
} ssl_t;

DEFINE_INTERFACE (SSLConnector) {
	void       (*destroy)        (ssl_t **);
	void       (*destroyContext) (ssl_context_t **);

    void       (*free)           (ssl_t **);

	ssl_context_t *(*context) (ssl_mode_t mode,
							   const char *certfile,
							   const char *keyfile,
							   const char *password,
							   const char *ca_list,
							   const char *ciphers,
							   const char *client_auth,
							   const char *dhfile);
	
	ssl_t     *(*connect)    (ssl_context_t *ctx, tcp_t *);
	ssl_t     *(*accept)     (ssl_context_t *ctx, tcp_t *);

	socket_t  *(*listen)     (u_int16_t port);

	uint32_t (*read)    (ssl_t *, char **buf, uint32_t size);
	uint32_t (*write)   (ssl_t *, char  *buf, uint32_t size);
	int     (*pending) (ssl_t *);
	void     (*setTimeout) (ssl_t *, int secs);	
	char      *(*getPeerCname) (ssl_t *);
	void       (*useRecords) (ssl_t *, boolean_t flag);
};

#endif 
