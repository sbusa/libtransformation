#ifndef __corenova_data_certstore_H__
#define __corenova_data_certstore_H__

#include <corenova/interface.h>
#include <corenova/data/cache.h>
#include <corenova/net/ssl.h>
#include <corenova/sys/quark.h>
#include <openssl/pem.h>

typedef struct {
	cache_t     *sslCache;
	char        *storeFile;
	int	    dumpInterval;
	X509	    *privateKey;
	quark_t     *loadQuark;
	quark_t     *dumpQuark;
} certStore_t;
/*//////// MODULE DECLARATION //////////////////////////////////////////*/

DEFINE_INTERFACE (CertStore)
{
    certStore_t *(*new)     (cache_t * cache, char *fileName, X509 *privateKey);
    void      (*load)     (certStore_t *store);
    void      (*destroy)     (certStore_t *store);
    void      (*dump)  (certStore_t *store);
    void      (*clean)  (certStore_t *store);
};


typedef struct {
	char *key;
	X509 *certificate;
} ssl_cache_entry_t;

DEFINE_INTERFACE (SSLCertCache) {
    cache_t		*(*new)		(uint32_t max_entries, uint32_t max_memory, void *cookie);
    ssl_cache_entry_t 	*(*put)		(cache_t *cache, char *dp,  const char *cname, X509 *certificate);
    ssl_cache_entry_t	*(*get)		(cache_t *cache, const char *cname);
    boolean_t           *(*delete)      (cache_t *cache, const char *cname);
    void 		*(*load)	(char *dp, cache_t *cache);
    void 		(*dump)	        (cache_t **cache, cache_dump_t *dump);
    void		*(*destroy)	(cache_t **);
};	
#endif
