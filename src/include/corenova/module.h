#ifndef __module_H__
#define __module_H__

#include <corenova/core.h>

#define MODULE_PATH_ENV	"NOVAMODULE_PATH"

#define MODULE_IMPLEMENTS_MAX 16
#define MODULE_REQUIRES_MAX   32
#define MODULE_OPTIONS_MAX    16
#define MODULE_TRANSFORMS_MAX 32

#define MODULE_NAME_MAXLEN    100

typedef void *            interface_t;
typedef struct __module * dependency_t;

typedef struct {
	const char *name;
	char *value;
	const char *desc;
} options_t;

#define OPTION(X,Y,Z) { X,Y,Z }
#define OPTION_NULL   { 0,0,0 }

typedef struct __module {
	
	/* public vars */
	const char *name;
	const char *version;
	const char *author;
	const char *description;
	char       *implements [MODULE_IMPLEMENTS_MAX];
	char       *requires   [MODULE_REQUIRES_MAX];
	options_t   options    [MODULE_OPTIONS_MAX];
    char       *transforms [MODULE_TRANSFORMS_MAX]; /* new option to specify supported transforms (use by xform engine) */
	
	/* internal vars */
	MUTEX_TYPE    *lock;
	lt_dlhandle   dll;
	uint32_t      accessCounter;
	interface_t   *interfaces; /* ptr to interfaces implemented by this module */
	dependency_t  *modules; /* ptr to modules required by this module */
	MUTEX_TYPE    cache_lock;
    void          *cache;   /* ptr to internal interface cache map */
    
} module_t;

#define THIS module_t LT_SYMBOL (this)
#define SELF &LT_SYMBOL (this)

#define MODULE_LOCK()   MUTEX_LOCK (*(LT_SYMBOL (this).lock))
#define MODULE_UNLOCK() MUTEX_UNLOCK (*(LT_SYMBOL (this).lock))

#endif	/* __module_H__ */
