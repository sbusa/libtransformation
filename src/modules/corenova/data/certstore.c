#include <corenova/source-stub.h>

THIS = {
	.version     = "1.0",
	.author      = "Ravi Chunduru <ravivsn@gmail.com>",
	.description = "This module enables Caching SSL Certificates.",
	.implements  = LIST ("CertStore", "SSLCertCache"),
	.requires    = LIST ("corenova.net.tcp",
			"corenova.net.ssl",	
			"corenova.data.string",
			"corenova.sys.quark",
			"corenova.data.cache")
};

#include <corenova/net/ssl.h>
#include <corenova/data/string.h>
#include <corenova/data/cache.h>
#include <corenova/data/certstore.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <errno.h>
#include <openssl/err.h> /* for ERR_print_errors */
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

char *
trim_cname(char *cname) {
	int ssize = 0;
	char *ocname = cname;
	while (*cname != '\0') {
		if (*cname == '\\') {
			break;
		}
		ssize++;
		cname++;
	}
	return strndup(ocname, ssize);
}

static inline int 
ssl_cert_entry_cmp (void *key, void *data) {
	char *A = (char *)key;
	char *B = ((ssl_cache_entry_t *)data)->key;
	DEBUGP (DINFO, "ssl_cert_entry_cmp", "searching for %s entry has %s", A, B);
	return strncmp(A, B, strlen(key));
}

static inline void
delete_cache_file(char *cname, char *dp) {
	if (cname && dp) {
		char *fname = trim_cname(cname);
		char *name = I (String)->copy(dp);

		if (fname && name) {
			I (String)->join(&name, "/");
			I (String)->join(&name, fname);
			unlink(name);
			free(name);
			free(fname);
		}
	}

}

static inline void 
ssl_cert_entry_del (void *data, void *cookie) {
	ssl_cache_entry_t *entry = data;
	if (entry) {
		/* Delete the persistent cache in the filesystem */
		delete_cache_file(entry->key, (char *)cookie);
		DEBUGP (DINFO, "ssl_cert_entry_del", "Deleting cert with cname %s, filepath %s", entry->key, (char *)cookie);
		free(entry->key);
		X509_free(entry->certificate); 
		free(entry);
	}
}

void CONSTRUCTOR mySetup ()
{
}

void DESTRUCTOR myDestroy ()
{
}


static cache_t *
newSslCertCache (uint32_t max_entries, uint32_t max_memory, char *dp) {
	DEBUGP (DINFO, "newSSLCertCache", "Creating a new SSL Cache");
	return I (Cache)->new (ssl_cert_entry_cmp, ssl_cert_entry_del, max_entries, max_memory, (void *)dp);
}

void 
write_to_file(char *dp, ssl_cache_entry_t *entry) {
	if (dp && entry ) {
		char *fname = trim_cname(entry->key);
		char *name = I (String)->copy(dp);
		if (fname && name) {
			I (String)->join(&name, "/");
			I (String)->join(&name, fname);

			/* Delete any existing file */
			unlink(name);	
			FILE *fp = fopen(name, "w");
			if (fp) {
				if (!PEM_write_X509(fp, entry->certificate)) {
					DEBUGP (DINFO, "write_to_file", "Failed to write certificate with cname:%s", entry->key);
				}
				fclose(fp);
			} else {
				DEBUGP (DINFO, "write_to_file", "Failed to open file %s", name);
			}
			free(name);
			free(fname);
		}
	}

}


static ssl_cache_entry_t *
putSslCertCacheEntry (cache_t *ssl_cache, char *dp, const char *cname, X509 *certificate) {
	if (!ssl_cache) {
		return NULL;
	}
	if (cname && certificate) {
		ssl_cache_entry_t *entry = (ssl_cache_entry_t *) calloc(1, sizeof(ssl_cache_entry_t));
		if (entry) {
			entry->key = I (String)->copy (cname);
			X509 *cert = NULL;
			BIO *memory = BIO_new(BIO_s_mem());
			if (memory) {
				if (PEM_write_bio_X509(memory, certificate)) {
					cert = PEM_read_bio_X509(memory, &cert, NULL, NULL);
					if (cert) {
						entry->certificate = cert;
						I (Cache)->put (ssl_cache, (void *)entry->key, entry, sizeof(ssl_cache_entry_t) + sizeof(cname));
						DEBUGP (DINFO, "putSslCertCacheEntry", "Cached Certificate for cname %s", cname);
						write_to_file(dp, entry);
						BIO_free(memory);
						return entry;
					}
				}
				BIO_free(memory);
			}
		}
	}
	return NULL;
}

static ssl_cache_entry_t *
getSslCertCacheEntry (cache_t *ssl_cache, char *cname) {
	if (cname && ssl_cache) {
		ssl_cache_entry_t *entry = (ssl_cache_entry_t *)I (Cache)->get (ssl_cache, cname);
		if (entry) {
			DEBUGP (DINFO, "getSslCertCacheEntry", "Found matching entry with cname %s", entry->key);
			return entry;
		}
	} else {
		DEBUGP (DINFO, "getSslCertCacheEntry", "ssl_cache in null");
	}
	return NULL;
}

static boolean_t 
deleteSslCertCacheEntry (cache_t *ssl_cache, char *cname) {
	if (cname && ssl_cache) {
		ssl_cache_entry_t *entry = getSslCertCacheEntry(ssl_cache, cname);
		if (entry) {
			if (I (Cache)->delete (ssl_cache, cname)) {
			    DEBUGP (DINFO, "deleteSslCertCacheEntry", "Deleted Cert from cache with cname %s", cname);
			    X509_free(entry->certificate);
			    free(entry->key);
			    free(entry);
			    return TRUE;
            } else {
                DEBUGP (DINFO, "deleteSslCertCacheEntry", "Failed to delete the cache entry with cname %s", cname);
            }
		}
	}
	return FALSE;
}

static long
loadSslCertCache (char *dp, cache_t *ssl_cache, EVP_PKEY *genKey, EVP_PKEY *caKey, long serialNumber) {
	DEBUGP (DINFO, "loadSslCertCache", "loading SSL certs from Cache %s", dp);
	if (dp && ssl_cache) {
		struct dirent *file = NULL;
		FILE *fp = NULL;
		DIR * dir = opendir(dp);
		if (dir) {
			/* open the files in the directory and load them into cache */
			while ((file  = readdir(dir))) {
				/* skip current and prev directory */
				if ((!strcmp(file->d_name, ".")) || (!strcmp(file->d_name, ".."))) {
					continue;
				}
				char *filename = I (String)->copy(dp);
				I (String)->join(&filename, "/");
				I (String)->join(&filename, file->d_name);
				fp = fopen(filename, "rb");
				if (fp) {
					X509 *cert = NULL;
					char *key;
					char PeerCname[1024];
					cert = PEM_read_X509(fp, &cert, NULL, NULL);

					if (cert) {	
						/* Fetch the cname */
						memset (PeerCname,0, sizeof(PeerCname));
						X509_NAME_get_text_by_NID(X509_get_subject_name(cert),
								NID_commonName, PeerCname, 1024);
						key = I (String)->copy(PeerCname);
						DEBUGP (DINFO, "loadSslCertCache", "found cert file %s with cname %s", file->d_name, key);
						if (key) {
							/* Resign the cert */
							X509_set_pubkey(cert, genKey);	
                            ASN1_INTEGER_set(X509_get_serialNumber(cert), serialNumber);
							if(X509_sign(cert, caKey, EVP_sha1())) {
								putSslCertCacheEntry (ssl_cache, NULL, key, cert);
							}
						}
					}
					fclose(fp);
				} else {
					DEBUGP (DINFO, "loadSslCertCache", "Failed to open file %s, error %s", filename, strerror(errno));
				}
				free(filename);
                serialNumber++;
			}
			closedir(dir);
		} else {
            /* create the directory if not existing */
			DEBUGP (DINFO, "loadSslCertCache", "not able to open dir, error is %s", strerror(errno));
            if (mkdir(dp, S_IRWXU|S_IRGRP|S_IXGRP) != 0) {
                DEBUGP (DINFO, "loadSslCertCache", "Failed to create directory %s, error is %s", dp, strerror(errno));
            }

		}
	}
    return serialNumber;		

}

static void
destroySslCertCache (cache_t **ptr) {
	DEBUGP (DINFO, "destroySslCertCache", "Destroying the SSL cache");
	I (Cache)->destroy (ptr);
}

static void
sslCertDump (void *data) {
	char *cname = ((ssl_cache_entry_t *)data)->key;
	DEBUGP (DINFO, "sslCertDump", "key name is %s", cname);
}

static void
dumpSslCertCache (cache_t **ptr, cache_dump_t *dump) {
	DEBUGP (DINFO, "dumpSslCertCache", "dumping certs");
	I (Cache)->dump(ptr, dump);
}

IMPLEMENT_INTERFACE(SSLCertCache) = {
	.new =	newSslCertCache,
	.put = 	putSslCertCacheEntry,
	.get = 	getSslCertCacheEntry,
	.delete  = 	deleteSslCertCacheEntry,
	.load = loadSslCertCache,
	.dump = dumpSslCertCache,
	.destroy = destroySslCertCache
};

/* TBD - cert store using quarks */
static int
_get_cert_issuer(X509 *cakey, cert_issuer_t *ir) {

	return 1;
}


static int
_get_cert_subject(X509 *cert, cert_subject_t *sub) {
	int size = 0;
	char content[1028];

	X509_NAME *name = NULL;
	name = X509_get_subject_name(cert);

	/* Extract CN, O, L, ST, C */
	/* Verify the string size before writing into data structure */
	size = X509_NAME_get_text_by_NID(name, NID_commonName, NULL, 0); 
	if (size < sizeof(sub->CN)) {
		X509_NAME_get_text_by_NID(name, NID_commonName, content, sizeof(sub->CN));
	}
	size = X509_NAME_get_text_by_NID(name, NID_countryName, NULL, 0);
	if (size == 2) {
		X509_NAME_get_text_by_NID(name, NID_countryName, content, sizeof(sub->C));
	}
	size = X509_NAME_get_text_by_NID(name, NID_localityName, NULL, 0);
	if (size == sizeof(sub->L)) {
		X509_NAME_get_text_by_NID(name, NID_localityName , content, sizeof(sub->L));
	}
	size = X509_NAME_get_text_by_NID(name, NID_organizationName, NULL, 0);
	if (size == sizeof(sub->O)) {
		X509_NAME_get_text_by_NID(name, NID_organizationName, content, sizeof(sub->O));
	}
	size = X509_NAME_get_text_by_NID(name, NID_pkcs9_emailAddress , NULL, 0);
	if (size == sizeof(sub->email)) {
		X509_NAME_get_text_by_NID(name, NID_pkcs9_emailAddress, content, sizeof(sub->email));
	}
	return 1;

}

static void
dump_ssl_certs(void *data, void *cookie) {
	cache_t *sslcache = NULL;
	//char *cname = ((ssl_cache_entry_t *)data)->key;
	ssl_cache_entry_t *entry = (ssl_cache_entry_t *)data;
	certStore_t *store = (certStore_t *)cookie;
	if (cookie) {
		sslcache = store->sslCache;
	}
	if (entry && sslcache) {
		DEBUGP (DINFO, "dump_ssl_certs", "key name is %s", entry->key);
		cert_metadata_t  meta;
		memset(&meta, 0, sizeof(cert_metadata_t));
		/* Either ssl_cache_entry must have the metadata or should be derived from cert */
		meta.version = 2;
		_get_cert_subject(entry->certificate, &meta.subject);
		_get_cert_issuer(store->caKey, &meta.issuer);
		/* TBD */
	}
}

static boolean_t 
_dumpCache (void *certStore_p) {
	/* Dump the ssl cert Cache into filesystem  periodically*/
	cache_dump_t * dump = (cache_dump_t *)malloc(sizeof(cache_dump_t *));
	dump->cb = dump_ssl_certs;
	dump->cookie = (void *)certStore_p;
	while (1) {
		I(SSLCertCache)->dump (&((certStore_t *)certStore_p)->sslCache, dump);
		sleep(10);
	}
	/* Will not reach here */
	free(dump);
	return TRUE;

}

static cert_issuer_t  *
_get_issuer_details(X509 *caKey) {
	return NULL;
}

static certStore_t *
_newCertMetadataStore (cache_t * sslCache_p, char * storeFile, X509 *privateKey, X509 *caKey) {
	/* Create a new Metadata Store.
	 * Initalize Quark that signs the cert and loads into ssl Cache.
	 */
	DEBUGP (DINFO, "_newcertMetadataStore", "Initialized cert Metadata store");
	certStore_t * certStore_p = (certStore_t *)malloc (sizeof(certStore_t));
	certStore_p->sslCache = sslCache_p;
	certStore_p->storeFile = storeFile;
	certStore_p->privateKey = privateKey;
	certStore_p->issuer = _get_issuer_details(caKey);
	certStore_p->dumpQuark = I(Quark)->new (_dumpCache, certStore_p);
	//certStore_p->loadQuark = I(Quark)->new (loadCache, certStore_p);
	return certStore_p;

}

static void
_loadCertMetadataStore (certStore_t *certStore) {
	/* spin up the quark
	 * gets cert metadata, signs up the cert, loads them into the certs cache
	 * when done, clean up the quark
	 */
}

static void
_destroyCertMetadataStore (certStore_t *certStore) {
	/* destroy the quarks */ 
	I(Quark)->destroy (&certStore->dumpQuark);
	I(Quark)->destroy (&certStore->loadQuark);
}


static void
_dumpCertMetadataStore (certStore_t *certStore) {
	/* Dump the certs metadata from cache into filesystem*/
	I(Quark)->setname(certStore->dumpQuark, "dumpcerts");
	I(Quark)->spin(certStore->dumpQuark); 
}

static void
_cleanCertMetadataStore (certStore_t *certStore) {
	/* Free the certstore in the filesystem */
}

IMPLEMENT_INTERFACE(CertStore) = {
	.new  = _newCertMetadataStore,
	.load = _loadCertMetadataStore,
	.destroy = _destroyCertMetadataStore,
	.dump    = _dumpCertMetadataStore,
	.clean	 = _cleanCertMetadataStore
}; 
