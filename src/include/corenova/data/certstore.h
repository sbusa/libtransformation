#ifndef __corenova_data_certstore_H__
#define __corenova_data_certstore_H__

#include <corenova/interface.h>
#include <corenova/data/cache.h>
#include <corenova/net/ssl.h>
#include <corenova/sys/quark.h>
#include <openssl/pem.h>


#define MAX_CNAME_LEN 1024
#define MAX_ORGNAME_LEN 124
#define MAX_ORGUNIT_LEN 128
#define MAX_LOCALITY_LEN 128
#define MAX_COUNTRY_NAME 2
#define MAX_STATE_LEN 24
#define MAX_EMAIL_LEN 24

typedef struct {
    char  OU[MAX_ORGNAME_LEN+1];
    char  O[MAX_ORGNAME_LEN+1];
    char  CN[MAX_CNAME_LEN+1];
    char  C[MAX_COUNTRY_NAME+1];
} cert_issuer_t;

typedef struct {
    char notBefore[128];
    char notAfter[128];
} cert_validity_t;

typedef struct {
    char CN[MAX_CNAME_LEN+1];
    char O[MAX_ORGNAME_LEN+1];
    char L[MAX_LOCALITY_LEN+1];
    char ST[MAX_STATE_LEN+1];
    char C[MAX_COUNTRY_NAME+1];
    char email[MAX_EMAIL_LEN+1];
} cert_subject_t;

typedef struct {
    char basicConstraints[128];
    char keyUsage[128];
    char keyIdentifier[128];
    char certType[128];
} cert_extensions_t;

typedef struct {
    int   version;
    long  serialNumber;
    cert_subject_t subject;
    cert_issuer_t issuer;
    cert_extensions_t extensions;
}cert_metadata_t;

typedef struct {
	cache_t     *sslCache;
	char        *storeFile;
	int	        dumpInterval;
	X509	    *privateKey;
	X509	    *caKey;
	quark_t     *loadQuark;
	quark_t     *dumpQuark;
    cert_issuer_t *issuer;
} certStore_t;

/*//////// MODULE DECLARATION //////////////////////////////////////////*/

DEFINE_INTERFACE (CertStore)
{
    certStore_t *(*new)     (cache_t * cache, char *fileName, X509 *privateKey, X509 *caKey);
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
    long        (*load)	(char *dp, cache_t *cache, EVP_PKEY *genKey, EVP_PKEY *caKey, long serialNumber);
    void 		(*dump)	        (cache_t **cache, cache_dump_t *dump);
    void		*(*destroy)	(cache_t **);
};	
#endif
