#include <corenova/source-stub.h>

THIS = {
	.version     = "2.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "This module enables SSL-oriented network operations.",
	.implements  = LIST ("SSLConnector", "SSLCertCache"),
	.requires    = LIST ("corenova.net.tcp",
                             "corenova.data.string",
                             "corenova.data.cache")
};

#include <corenova/net/ssl.h>
#include <corenova/data/string.h>
#include <corenova/data/cache.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <errno.h>
#include <openssl/err.h> /* for ERR_print_errors */
#include <unistd.h>
#include <dirent.h>

/***** SSL Thread-Safe callback setup  *****/

static MUTEX_TYPE *mutexList = NULL;

static void _sslLockingFunction (int32_t mode, int32_t n, const char *file, int32_t line) {
	if (mode & CRYPTO_LOCK) {
		MUTEX_LOCK (mutexList[n]);
	}
	else {
		MUTEX_UNLOCK (mutexList[n]);
	}
}

static unsigned long _sslIdFunction (void) {
	return ((unsigned long)THREAD_ID);
}

static int32_t _sslThreadSetup(void)
{
	int32_t i;

	mutexList = (MUTEX_TYPE *) OPENSSL_malloc(CRYPTO_num_locks() * sizeof(MUTEX_TYPE));
	if (mutexList) {
		for (i=0; i < CRYPTO_num_locks(); i++){
			MUTEX_SETUP (mutexList[i]);
		}

		CRYPTO_set_id_callback(_sslIdFunction);
		CRYPTO_set_locking_callback(_sslLockingFunction);
		return 1;
	}
	return 0;
}

static int32_t _sslThreadCleanup(void)
{
	int32_t i;

	if (mutexList) {
		CRYPTO_set_id_callback (NULL);
		CRYPTO_set_locking_callback(NULL);
		for (i=0; i<CRYPTO_num_locks(); i++) {
			MUTEX_CLEANUP (mutexList[i]);
		}
		OPENSSL_free(mutexList);
		return 1;
	}
	return 0;
}

char *
trim_cname(char *cname) {
	int ssize;
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

static inline int ssl_cert_entry_cmp (void *key, void *data) {
	char *A = (char *)key;
	char *B = ((ssl_cache_entry_t *)data)->key;
	if (I (String)->equal (A, B)) {
		return 0;
	} else {
		return 1;
	}
}

static inline void ssl_cert_entry_del (void *data, void *cookie) {
	ssl_cache_entry_t *entry = data;
	if (entry) {
		DEBUGP (DINFO, "ssl_cert_entry_del", "Deleting cert with cname %s, filepath %s", entry->key, (char *)cookie);
		free(entry->key);
		X509_free(entry->certificate);
		free(entry);
	}
}

void CONSTRUCTOR mySetup ()
{
//	DEBUGP(DINFO,"constructor"," - making SSL thread-safe - ");
	SSL_load_error_strings ();                /* readable error messages */
	SSL_library_init ();                      /* initialize library */
//	actions_to_seed_PRNG ();
	
	_sslThreadSetup ();
}

void DESTRUCTOR myDestroy ()
{
	_sslThreadCleanup ();
}

/***** Helper routines *****/

static const char *PrivateKeyPassword; /* private password for opening enc key */

static int32_t password_cb(char *buf, int32_t len, int32_t rwflag, void *userdata)
{
	if (len < strlen(PrivateKeyPassword)+1) return 0;

	strncpy(buf,PrivateKeyPassword,len);
	return (strlen(buf));
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
		I (String)->join(&name, "/");
		I (String)->join(&name, fname);
		
		FILE *fp = fopen(name, "w");
		if (fp) {
			PEM_write_X509(fp, entry->certificate);
			fclose(fp);
			free(name);
			free(fname);
		} else {
			DEBUGP (DINFO, "write_to_file", "Failed to open file %s", name);
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
			entry->certificate = certificate;
			I (Cache)->put (ssl_cache, (void *)entry->key, entry, sizeof(ssl_cache_entry_t) + sizeof(cname));
			write_to_file(dp, entry);
			return entry;
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

static void
loadSslCertCache (char *dp, cache_t *ssl_cache) {
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
					char * PeerCname = (char *)malloc(1024);
					cert = PEM_read_X509(fp, &cert, NULL, NULL);
					/* Fetch the cname */
               		         	memset (PeerCname,0,sizeof (PeerCname));
	       		                X509_NAME_get_text_by_NID(X509_get_subject_name(cert),
               		                                                           NID_commonName, PeerCname, 1024);
					key = I (String)->copy(PeerCname);
					DEBUGP (DINFO, "loadSslCertCache", "found cert file %s with cname %s", file->d_name, key);
				 	if (cert && key) {	
						putSslCertCacheEntry (ssl_cache, NULL, key, cert);
					}
					free(PeerCname);
					fclose(fp);
			        } else {
					DEBUGP (DINFO, "loadSslCertCache", "Failed to open file %s, error %s", filename, strerror(errno));
				}
				free(filename);
			}
			closedir(dir);
		} else {
			DEBUGP (DINFO, "loadSslCertCache", "not able to open dir");
		}
	}		
				
}

static void
destroySslCertCache (cache_t **ptr) {
	 DEBUGP (DINFO, "destroySslCertCache", "Destroying the SSL cache");
	I (Cache)->destroy (ptr);
}

IMPLEMENT_INTERFACE(SSLCertCache) = {
	.new =	newSslCertCache,
	.put = 	putSslCertCacheEntry,
	.get = 	getSslCertCacheEntry,
	.load = loadSslCertCache,
	.destroy = destroySslCertCache
};

/** returns: CN_OK on success, else CN_ERR **/
static boolean_t
_verifyCertificate(SSL *ssl) {
	int32_t check = SSL_get_verify_result(ssl);

	DEBUGP(DDEBUG,"verifyCertificate","entering CERT verification routine...");
	switch (check){
      case X509_V_OK:
      case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
          /* verify ok */
          break;
      default:
          DEBUGP(DERR,"verifyCertificate","FAILED to verify CERT!  EC(%d)",check);
          return FALSE;
	}
	DEBUGP(DDEBUG,"verifyCertificate","OK.");
	return TRUE;
}

static boolean_t
_loadDh(SSL_CTX *ctx, const char *file)
{
	BIO *bio = BIO_new_file (file,"r");
	if (bio) {
		DH *dh= PEM_read_bio_DHparams(bio,NULL,NULL,NULL);
		BIO_free (bio);
		if (SSL_CTX_set_tmp_dh (ctx,dh)>=0) {
			return TRUE;
		} else {
			DEBUGP(DERR,"loadDh","couldn't set DH parameters");
		}
	} else {
		DEBUGP (DERR,"loadDh","couldn't open DH file (%s)",file);
	}
	return FALSE;
}

static int32_t _checkSSLError (SSL *ssl, int32_t code) {
    
    switch(SSL_get_error(ssl,code)){
      case SSL_ERROR_NONE: 
          return SSL_SUCCESS;
		
      case SSL_ERROR_WANT_READ:  /* go back and read more ... */
      case SSL_ERROR_WANT_WRITE: /* go back and write more ... */
          return SSL_TRY_AGAIN;
		
      case SSL_ERROR_ZERO_RETURN: DEBUGP(DDEBUG,"checkSSLError","ZERO RETURN");
          return SSL_SHUTDOWN;

      case SSL_ERROR_SYSCALL:
           /* This case is not error. When ERR_get_error() does not report any error return
           * with what we have.
           */  
          if (!ERR_get_error() && !errno) {
              DEBUGP (DDEBUG, "checkSSLError", "returning with no data");
              return SSL_RETURN_WITH_DATA;
          }

          DEBUGP(DFATAL,"checkSSLError","SYSCALL (ERR_get_error: %lu, ERR_last_error: %lu, ERRNO: %d)", ERR_get_error (), ERR_peek_last_error (), errno);
          return SSL_SHUTDOWN;

          /* 
           * if(SystemExit || errno == EPIPE) {
           *     DEBUGP(DFATAL,"checkSSLError","SYSCALL");
           *     return SSL_SHUTDOWN;
           * }
           * return SSL_TRY_AGAIN;
           */
		
      default:
          DEBUGP(DERR,"checkSSLError","unknown error (%d) and code is %d",SSL_get_error(ssl,code), code);
	}
	
	return SSL_SHUTDOWN;
}

/*//////// API ROUTINES //////////////////////////////////////////*/

static void
destroySSL (ssl_t **sslPtr) {
	if (sslPtr) {
		ssl_t *ssl = *sslPtr;

		if (ssl) {
			if (ssl->conn) {
				SSL_shutdown (ssl->conn);
				SSL_free (ssl->conn);
			}
			if (ssl->ctx){
				SSL_CTX_free(ssl->ctx);
			}
			/* SSL_free indirectly removes it, damn this was causing the problem... */
//			BIO_free (ssl->sslBio);
//			
//			no longer will this destroy the TCP connection underneath it.
//			I (TcpConnector)->destroy (&ssl->tcp);
			free (ssl);
			*sslPtr = NULL;
		}
	}
}

/*
 * This behavior is similar to destroy except we ONLY destroy the SSL structures.
 */ 
static void
freeSSL (ssl_t **sslPtr) {
    if (sslPtr) {
        ssl_t *ssl = *sslPtr;
        if (ssl) {
            if (ssl->conn) SSL_free (ssl->conn);
            ssl->conn = NULL;
            free (ssl);
            *sslPtr = NULL;
        }
    }
}

static void _destroyContext (ssl_context_t **ctx) {
	if (ctx && *ctx) {
		SSL_CTX_free (*ctx);
		*ctx = NULL;
	}
}

/**
 * - initializes the SSL context
 * - sets up all reading of certificates
 * - reading private keys
 * - loading trusted CAs
 * - setting our cipher suite.
 **/
static ssl_context_t *
_newContext (ssl_mode_t mode,
			 const char *certfile,
			 const char *keyfile,
			 const char *password,
			 const char *ca_list,
			 const char *ciphers,
                         const char *client_auth,
			 const char *dhfile) {
        boolean_t clientAuth = 0;
	ssl_context_t *ctx = NULL;
	SSL_METHOD *method = NULL;


	switch (mode) {
      case SSL_CLIENT:
          method = SSLv23_client_method ();
          break;
      case SSL_SERVER:
          method = SSLv23_server_method ();
          break;
      default:
          DEBUGP (DERR,"newContext","unknown context mode passed in!");
          return NULL;
	}
	ctx = SSL_CTX_new (method);
	if (ctx) {
		/* hack for ciphers for now... */
		ciphers  = ciphers?ciphers:SSL_default_ciphers;
		password = password?password:"";

		DEBUGP (DINFO,"newContext","loading certfile chain: %s",certfile);
		if(!(SSL_CTX_use_certificate_chain_file(ctx,certfile))){
			DEBUGP(DERR,"newContext","failed using chained certificate!");
			_destroyContext (&ctx);
			return NULL;
		}

		MODULE_LOCK ();
		DEBUGP (DINFO,"newContext","opening encrypted keyfile: %s",keyfile);
		PrivateKeyPassword = password;
		SSL_CTX_set_default_passwd_cb(ctx, (pem_password_cb *)password_cb);
		if(!(SSL_CTX_use_PrivateKey_file(ctx, keyfile, SSL_FILETYPE_PEM))){
			DEBUGP(DERR,"newContext","failed opening certificate with password!");
			_destroyContext (&ctx);
			return NULL;
		}
		MODULE_UNLOCK ();

		DEBUGP(DINFO,"newContext","loading trusted CAs from %s",ca_list);
		if (ca_list) {
			if(!(SSL_CTX_load_verify_locations(ctx, ca_list, 0))){
				DEBUGP (DERR,"newContext","failed to load the CA list!");
				_destroyContext (&ctx);
				return NULL;
			}
		}
		DEBUGP(DINFO,"newContext","setting up ciphers [%s]",ciphers);
		SSL_CTX_set_cipher_list(ctx,ciphers);

		/* set compression options? */

		/* setup DH params */
		if (mode == SSL_SERVER) {
			DEBUGP(DINFO,"newContext","setting up dh parameters (%s)",dhfile);
			if (!_loadDh(ctx,dhfile)){
				DEBUGP (DERR,"newContext","failed to load the dh file!");
				_destroyContext (&ctx);
				return NULL;
			}
                        /* clientAuth is from configuration. */
                        if (client_auth) {
                            clientAuth = atoi(client_auth);
                            clientAuth = 0; /* For now zero; since no call back to verify client certificate */
                        }  
                        if (clientAuth) {
				SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,0);
                        } else {
				SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, 0);
                        }

                        DEBUGP (DINFO,"newContext","disable any session caching!");
                        SSL_CTX_set_session_cache_mode (ctx, SSL_SESS_CACHE_OFF);
		}
	} else {
		DEBUGP (DERR,"newContext","failed to initialize the SSL_CTX structure!");
	}
	return (ctx);
}

/*
  - calls dependent 'tcp' module to establish TCP connection.
  - wraps that connection into an SSL context.
*/
static ssl_t *
_connect (ssl_context_t *ctx, tcp_t *tcp) {
	ssl_t *ssl = (ssl_t *)calloc (1,sizeof (ssl_t));
        int retvalue = 0;
	if (ssl) {
		
		ssl->tcp = tcp;
		if (ssl->tcp) {
			int32_t skd = tcp->sock->skd;
			DEBUGP (DINFO,"connect","attempting SSL connection on socket #%d",skd);
			ssl->conn = SSL_new (ctx);
			ssl->sslBio = BIO_new_socket (skd, BIO_NOCLOSE);
			
			DEBUGP (DINFO, "connect", "SSL negotiate timeout set to 60 secs");
                        I (TcpConnector)->setTimeout (ssl->tcp, 60);

			SSL_set_bio (ssl->conn,ssl->sslBio,ssl->sslBio);
                        retvalue = SSL_connect (ssl->conn);
                        switch (_checkSSLError (ssl->conn, retvalue)) {
                             case SSL_SHUTDOWN:
                             case SSL_TRY_AGAIN:
                                 I (SSLConnector)->destroy (&ssl);
                                 break;
                             case SSL_SUCCESS:
                             case SSL_RETURN_WITH_DATA:
                                  if (_verifyCertificate (ssl->conn)) {
                                      I (TcpConnector)->setTimeout (ssl->tcp, 0);
                                  } else {
                                      I (SSLConnector)->destroy (&ssl);
                                  }
                                  break;  
                        }
                        DEBUGP (DINFO, "connect", "SSL_connect returned %d", retvalue);
#if 0
			if (retvalue  > 0) {

				if (_verifyCertificate (ssl->conn)) {
                    
					/* XXX - DO NOT SAVE THIS SESSION!
                     * 
                     * DEBUGP (DDEBUG,"connect","saving this connection for future use!");
                     * ssl->session = SSL_get1_session (ssl->conn);
                     */
                    
                                       I (TcpConnector)->setTimeout (ssl->tcp, 0);
				} else {
					I (SSLConnector)->destroy (&ssl);
				}
			} else {
                                DEBUGP (DDEBUG, "connect", "SSL has the following set conn");
                                DEBUGP (DDEBUG, "connect", "conn %x ssl %x", ssl->conn, ssl);
                                if (retvalue < 0) {
				    I (SSLConnector)->destroy (&ssl);
                                    DEBUGP (DDEBUG, "connect", "conn %x ssl %x", ssl->conn, ssl);
                                    return NULL;
                                } else {
                                    /* Ravi: SSL is already shutdown, we just need to free ssl */
                                    return NULL;
                                }  
			}
#endif
		} else {
			DEBUGP (DERR,"connect","unable to establish underlying TCP connection!");
			I (SSLConnector)->destroy (&ssl);
		}
	}
	return ssl;
}

/*
  wrapper around TcpConnector->listen
*/
static socket_t *
sslListen (u_int16_t port) {
	return I (TcpConnector)->listen (port);
}

/**
 * accept()
 * --------
 *
 * wrap TCP object with SSL context, and readify the self for I/O.
 *
 * returns: new ssl_t on success, else NULL.
 **/
static ssl_t *
sslAccept (ssl_context_t *ctx, tcp_t *tcp) {
	ssl_t *ssl = (ssl_t *)calloc (1,sizeof (ssl_t));
	int retval = 0;
	if (ssl) {
		ssl->tcp = tcp;

		if (ssl->tcp) {
			int32_t skd = tcp->sock->skd;
			char maxretry = 3;

			DEBUGP (DINFO,"accept","establishing SSL connection on socket #%d",skd);

			ssl->conn = SSL_new (ctx);
			if (!ssl->conn) {
				DEBUGP (DERR,"accept","cannot create new SSL connection from context!");
                                free(ssl);
                                return NULL;
			}

			DEBUGP(DINFO, "accept", "SSL accept timeout set to 10 secs");
			I (TcpConnector)->setTimeout(ssl->tcp, 10);

			ssl->sslBio = BIO_new_socket (skd, BIO_NOCLOSE);
                        if (ssl->sslBio <= 0) {
                            DEBUGP (DERR, "sslAccept", "BIO_new_socket failed");
                        }    
                            
			SSL_set_bio (ssl->conn,ssl->sslBio,ssl->sslBio);

try_ssl_accept:

			if (maxretry--) {
				// clear the error queue...
				while (ERR_get_error ()) { ; }

				retval = SSL_accept (ssl->conn);
				switch (_checkSSLError (ssl->conn,retval)) {
					case SSL_TRY_AGAIN:
						usleep (10000); /* slow down & try again */
						DEBUGP (DDEBUG,"accept","trying SSL accept again...");
						goto try_ssl_accept;

					case SSL_SUCCESS:
						if (_verifyCertificate (ssl->conn)) {
							//DEBUGP (DINFO,"accept","setting connection mode to auto retry");
							//SSL_set_mode (ssl->conn, SSL_MODE_AUTO_RETRY); /* blocking call */

							/* XXX - DO NOT SAVE THIS SESSION!
							 * 
							 * DEBUGP (DDEBUG,"accept","saving this connection for future use!");
							 * ssl->session = SSL_get1_session (ssl->conn);
							 */

							I (TcpConnector)->setTimeout(ssl->tcp, 0);
							return ssl;

						} else {
							DEBUGP (DERR,"accept","unable to verify cert (%d): %s ",SSL_get_error (ssl->conn,retval), ERR_error_string (ERR_get_error (),NULL));
							I (SSLConnector)->destroy (&ssl);
						}
						break;
				}
                                //DEBUGP (DERR, "accept", "ssl->conn is %x", (unsigned int)ssl->conn);
				DEBUGP (DERR,"accept","unable to accept connection: %d",SSL_get_error (ssl->conn,retval));
				I (SSLConnector)->destroy (&ssl);
                                return NULL;
			} else {
				DEBUGP (DERR,"accept","cannot retry anymore!");
				I (SSLConnector)->destroy (&ssl);
			}

		} else {
			DEBUGP (DERR,"accept","unable to establish underlying TCP connection!");
			I (SSLConnector)->destroy (&ssl);
		}
	}
	return ssl;
}

static char *
_getPeerCname(ssl_t *ssl)
{
	static char PeerCname[256]; /* locked access only! */

	if (ssl && ssl->conn) {
		X509 * peerCert = SSL_get_peer_certificate(ssl->conn);
		if (peerCert) {
			char * cname = NULL;
			MODULE_LOCK ();
			memset (PeerCname,0,sizeof (PeerCname));
			X509_NAME_get_text_by_NID(X509_get_subject_name(peerCert),
									  NID_commonName, PeerCname, 256);
			cname = strdup (PeerCname);
			MODULE_UNLOCK ();

            X509_free (peerCert); /* bah, a memory leak fixed here... */
			return cname;
		} else {
			DEBUGP (DERR,"getPeerCname","unable to retrieve peer certificate!");
		}
	}
	return NULL;
}

/*
  !IMPORTANT! (*buf) must be set to NULL if you want this call to CALLOC!

  we can read upto 2^32 bytes into this call.
  NOTE: MAX_REC_SIZE is 16384, so we need to break it up.

  returns: read bytes on success, else 0.
*/
static uint32_t
sslRead (ssl_t *ssl, char **buf, uint32_t size) {
	uint32_t read = 0;
	boolean_t dynamicBuffer = FALSE;
	int32_t returnValue = 0;  /* must be signed int */
	if (ssl && ssl->conn && buf && size) {
		/* do records read/write */
		if (ssl->records) {
		
			DEBUGP(DDEBUG, "sslRead", "using records");
		
			uint32_t reqSize = size;
			//uint32_t tmp_size = 0;
try_record_read:
			returnValue = SSL_read (ssl->conn, &size, sizeof (size));
			DEBUGP(DDEBUG, "sslRead", "received %u record size", size);
			size = ltohel(size); // big-endian magic
			DEBUGP(DDEBUG, "sslRead", "converted record size from little endian to host: %u", size);

			switch (_checkSSLError (ssl->conn,returnValue)) {
				case SSL_TRY_AGAIN:
					usleep (10000);	/* slow down & try again */
					goto try_record_read;
				case SSL_SHUTDOWN:
					DEBUGP (DERR,"sslRead","fatal error during record read.");
					return 0;
			}
			if (size > reqSize) {
				DEBUGP (DERR,"sslRead","record size [%lu] > requested size [%lu]",(unsigned long)size,(unsigned long)reqSize);
				return 0;
			}
		} else {
		
		    DEBUGP(DDEBUG, "sslRead", "not using records");		
		
		}
		if (!*buf) {
			dynamicBuffer = TRUE;
			/*
			   Here, we allocate NOT the actual recSize, but recSize+1, and
			   terminate the buffer with '\0'.
			 */
			//			DEBUGP (DINFO,"sslRead","dynamically allocating %lu bytes to hold result",size+1);
			*buf = (char *) calloc (size+1,sizeof (unsigned char));
			if (*buf) {
				(*buf)[size] = '\0'; /* null terminated! */
			} else {
				DEBUGP (DERR,"sslRead","cannot allocate memory for %lu bytes!",(unsigned long)size+1);
				return 0;
			}
		}

		while (size) {
			uint32_t toRead = (size > MAX_REC_SIZE)?MAX_REC_SIZE:size;
			DEBUGP (DDEBUG, "sslRead", "size to read %lu bytes", (unsigned long)toRead);
try_read:
			returnValue = SSL_read (ssl->conn,(*buf)+read, toRead);
			DEBUGP (DDEBUG, "sslRead", "read %d bytes", returnValue); 
			if ((returnValue > 0) && (returnValue < size)) return (returnValue);  /* This is required. Else, sslRead goes into a loop */
			
			int ssl_error_num;
			
			switch (ssl_error_num = _checkSSLError (ssl->conn,returnValue)) {
				case SSL_TRY_AGAIN:
					usleep (10000); /* slow down & try reading again... */
					goto try_read;
				case SSL_SHUTDOWN:
					DEBUGP (DERR,"sslRead","fatal error during read.");
					if (dynamicBuffer) free (*buf);
					return 0;
				case SSL_RETURN_WITH_DATA:
					read += returnValue;
					return read;
			}

			read += returnValue;
			size -= returnValue;
			
		}
	}
	
	return read;
}

/*
  writes a data item to the encrypted stream. upto defined 'size'
  from the data ptr.

  we can write upto 2^32 bytes into this call.
  NOTE: MAX_REC_SIZE is 16384, so we need to break it up.
 
  returns: write bytes on succes, else 0.
*/
static uint32_t 
sslWrite (ssl_t *ssl, char *buf, uint32_t size) {
	uint32_t write = 0;
	uint32_t returnValue = 0;
	if (ssl && ssl->conn && buf && size) {
		SSL_set_mode (ssl->conn, SSL_MODE_AUTO_RETRY); /* blocking call */

		if (ssl->records) {
		
		    DEBUGP(DDEBUG, "sslWrite", "using records");

		    uint32_t tmp_size = htolel(size); // big-endian magic

            DEBUGP(DDEBUG, "sslWrite", "writing %u bytes with records", size);

		  try_record_write:
		  
			returnValue = SSL_write (ssl->conn,&tmp_size,sizeof (tmp_size));
			switch (_checkSSLError (ssl->conn,returnValue)) {
              case SSL_TRY_AGAIN:
                  usleep (10000);
                  goto try_record_write;
              case SSL_SHUTDOWN:
                  return -1;
			}
		} else {
		
		    DEBUGP(DDEBUG, "sslWrite", "not using records");
		
		}
		
		while (size) {
			int32_t toWrite = (size > MAX_REC_SIZE)?MAX_REC_SIZE:size;
		  try_write:
			returnValue = SSL_write (ssl->conn,buf+write,toWrite);
			switch (_checkSSLError (ssl->conn,returnValue)) {
              case SSL_TRY_AGAIN:
                  usleep (10000);
                  goto try_write;
				
              case SSL_SHUTDOWN:
                  return -1;
			}
			/* should check returnValue or assume returnValue == toWrite here? */
			write += toWrite;
			size  -= toWrite;
		}
	}
	return write;
}

static void
_setTimeout(ssl_t *ssl, int secs) {

    if(ssl) {
        I (TcpConnector)->setTimeout(ssl->tcp, secs);
    }

}

static void
_useRecords(ssl_t *ssl, boolean_t flag) {

    if(ssl) {
    
	DEBUGP(DDEBUG, "useRecords", "use of records %s", flag ? "ENABLED" : "DISABLED");
    
	ssl->records = flag;
    
    }

}

IMPLEMENT_INTERFACE (SSLConnector) = {
	.context = _newContext,
	.connect = _connect,
	.listen  = sslListen,
	.accept  = sslAccept,
	.read    = sslRead,
	.write   = sslWrite,

	.getPeerCname   = _getPeerCname,

	.destroy        = destroySSL,
        .free           = freeSSL,
	.setTimeout     = _setTimeout,
	.destroyContext = _destroyContext,
	.useRecords     = _useRecords
};
