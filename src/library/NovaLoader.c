/*
  nova-loader : brought to you by Peter K. Lee <saint@corenova.com>
  
 * Description

  This is the MAIN API for module loading and module access.
 
  Borrowed heavily from SIC's dynamic module loader (written by Gary
  V. Vaughan).  I've made a transition to libtool, and Gary helped write
  libtool, so I'm sure he had a good idea about how to use it
  effectively...

 * Usage

  Just link the generated library to the program you're building, and issue
  nova_load(NULL) to get the ball rolling!
  
 * Features
 
  - module dependency support
  - protected symbols
  - easy calling interface to modules
  
 */

#include <corenova/module.h>
#include <corenova/debug.h>
#include <unistd.h>
#include <ctype.h>              /* for isalnum */


static MUTEX_TYPE __lock__;

#define LOADER_LOCK()   MUTEX_LOCK   (__lock__)
#define LOADER_UNLOCK() MUTEX_UNLOCK (__lock__)

#undef  DEBUG_SOURCE
#define DEBUG_SOURCE "NovaLoader"

#ifdef __DEBUG__
int32_t DebugLevel = __DEBUG__;
#else
int32_t DebugLevel = DOFF;
#endif

int32_t QuarkCount = 0;
boolean_t SystemExit = FALSE;
boolean_t NovaCache = FALSE;

static boolean_t nova_loader_initialized = FALSE;
static module_t *self = NULL;

/* function prototypes */
void nova_init(void);
void nova_exit(void);
module_t *nova_load(const char *name);
void nova_unload(module_t *);
void *nova_symbol(module_t *, const char *sym);

/***** Main Code *****/

void CONSTRUCTOR __init__(void) {
    nova_init();
}

void DESTRUCTOR __exit__(void) {
    nova_exit();
}

void
nova_init(void) {
    if (!nova_loader_initialized) {
        nova_loader_initialized = TRUE;
        DEBUGP(DINFO, "nova_init", "firing up the system... %u", (unsigned int) getppid());

#ifdef USE_PREOPEN
        LTDL_SET_PRELOADED_SYMBOLS();
#endif
        MUTEX_SETUP(__lock__);
        if (lt_dlinit()) {
            DEBUGP(DERR, "nova_init", "unable to initialize libtool dynamic loader! (%s)", lt_dlerror());
            exit(1);
        }
        lt_dladdsearchdir("./");
        lt_dladdsearchdir(getenv(MODULE_PATH_ENV));
        lt_dladdsearchdir(DEFAULT_NOVAMODULE_PATH);
        DEBUGP(DINFO, "nova_init", "search directory set to: %s", lt_dlgetsearchpath());

        /* load self! */
        if ((self = nova_load(NULL))) {
            DEBUGP(DINFO, "nova_init", "'%s' loaded cleanly...", self->name);
            fprintf(stderr, "\n");
        } else {
            DEBUGP(DERR, "nova_init", "cannot load self!");
            exit(1);
        }
    }
}

void
nova_exit(void) {
    if (nova_loader_initialized) {
        /* unload self! */
        if (self) {
            fprintf(stderr, "\n");
            DEBUGP(DINFO, "nova_exit", "'%s' unloading...", self->name);
            nova_unload(self);
            self = NULL;
        }

        MUTEX_CLEANUP(__lock__);
        lt_dlexit();

        DEBUGP(DFATAL, "nova_exit", "system terminated... %u", (unsigned int) getppid());
        nova_loader_initialized = FALSE;
    }
}

static lt_dlhandle
nova_dlopen(const char *name) {
    if (name) {

        char *nameCopy = strdup(name);
        if (nameCopy) {
            char *ptr = NULL;
            /*
              if the name includes a '.' we run it ourselves through the
              search path.
             */
            if ((ptr = strchr(nameCopy, '.'))) {
                lt_dlhandle dll;
                *ptr = '/';
                /* convert the rest of '.' into '/' */
                while ((ptr = strchr(ptr + 1, '.'))) {
                    *ptr = '/';
                }

                /* first try to open using the provided name */
                dll = lt_dlopenext(nameCopy);

                if (!dll) { /* we run this through the searchpath */
                    const char *searchPath = lt_dlgetsearchpath();

                    char *searchPathCopy = searchPath ? strdup(searchPath) : NULL;
                    if (searchPathCopy) {
                        static char fullModuleName[1024]; /* locked access only! */
                        char *searchDir = searchPathCopy;
                        while ((ptr = strchr(searchDir, ':'))) {
                            *ptr = '\0';
                            LOADER_LOCK();
                            fullModuleName[0] = '\0';
                            strcat(fullModuleName, searchDir);
                            strcat(fullModuleName, "/");
                            strcat(fullModuleName, nameCopy);

                            dll = lt_dlopenext(fullModuleName);
                            LOADER_UNLOCK();

                            if (dll) break;

                            searchDir = ptr + 1;
                        }

                        if (!dll) { /* still haven't found it... */
                            LOADER_LOCK();
                            fullModuleName[0] = '\0';
                            strcat(fullModuleName, searchDir);
                            strcat(fullModuleName, "/");
                            strcat(fullModuleName, nameCopy);
                            dll = lt_dlopenext(fullModuleName);
                            LOADER_UNLOCK();
                        }

                        free(searchPathCopy);
                    }
                }

                /* return whatever dll we found, even if NULL */

                free(nameCopy);

                return dll;
            }
            free(nameCopy);
        }
    }
    /* default fallback */
    return lt_dlopenext(name);
}

static void
_incrementAccess(module_t *m) {
    if (m->modules) {
        int32_t idx = 0;
        while (m->modules[idx]) {
            _incrementAccess(m->modules[idx]);
            idx++;
        }
    }
    m->accessCounter++;
}

static char *
_canonicalize(const char *string) {
    if (string) {
        size_t i, len = strlen(string);
        char *copy = strdup(string);
        for (i = 0; i < len; i++) {
            if (!isalnum((int) string[i])) {
                copy[i] = '_';
            }
        }
        return copy;
    }
    return NULL;
}

static char *
_extractPrefix(const char *string) {
    if (string) {
        char *prefix = NULL;
        char *copy = strdup(string);
        char *ptr = NULL;
        if ((ptr = strrchr(copy, '.'))) {
            *(ptr + 1) = '\0';
        }
        prefix = _canonicalize(copy);
        free(copy);
        //DEBUGP (DDEBUG,"extractPrefix","prefix: %s",prefix)
        return prefix;
    }
    return NULL;
}

/*
 * XXX - this is a MAJOR MAJOR HACK
 * 
 * basically, if the underlying LTDL library makes any changes to
 * these following structures, we are TOAST!
 *
 * The proper way would be to register our own user-defined loader (in
 * order to utilize the sym_prefix capability), but this is being done
 * because I am a lazy f*@#.
 */

/* This type is used for the array of caller data sets in each handler. */

#ifndef LTDL_OLD
typedef unsigned lt_dlcaller_id;
#endif

typedef struct {
    lt_dlcaller_id key;
    lt_ptr data;
} lt_caller_data;

/* This structure is used for the list of registered loaders. */
struct lt_dlloader {
    struct lt_dlloader *next;
    const char *loader_name; /* identifying name for each loader */
    const char *sym_prefix; /* prefix for symbols */
    lt_module_open *module_open;
    lt_module_close *module_close;
    lt_find_sym *find_sym;
    lt_dlloader_exit *dlloader_exit;
    lt_user_data dlloader_data;
};

struct lt_dlhandle_struct {
    struct lt_dlhandle_struct *next;
    struct lt_dlloader *loader; /* dlopening interface */
    lt_dlinfo info;
    int depcount; /* number of dependencies */
    lt_dlhandle *deplibs; /* dependencies */
    lt_module module; /* system module handle */
    lt_ptr system; /* system specific data */
    lt_caller_data *caller_data; /* per caller associated data */
    int flags; /* various boolean stats */
};

void *
nova_symbol(module_t *module, const char *sym) {

    if (module && module->dll && sym) {
#ifdef LTDL_OLD
        lt_dlloader *loader = module->dll->loader;
#else
        lt_dlvtable *loader = (lt_dlvtable *) lt_dlloader_get(module->dll);
#endif
        void *symbol = NULL;

        loader->sym_prefix = _extractPrefix(module->name);
        symbol = lt_dlsym(module->dll, sym);
       if (loader->sym_prefix) {
            free((char *) loader->sym_prefix);
            loader->sym_prefix = NULL;
        }
        return symbol;
    }
    return NULL;
}

// use LGPL open-source implementation of "splay tree" cache
#include <ubiqx/library/ubi_Cache.h>
#include <corenova/interface.h>

typedef struct {

    ubi_cacheEntry node;
    char *key;
    interface_t *value;
    
} interface_entry_t;


static inline int interface_entry_cmp (ubi_trItemPtr key, ubi_trNodePtr data) {
    char *A = (char *)key;
    char *B = ((interface_entry_t *)data)->key;
    //DEBUGP (DDEBUG,"interface_entry_cmp","comparing '%s' with '%s' at %p",A,B, data);
    return strncmp (A?A:"",B?B:"",INTERFACE_NAME_MAXLEN);
}

static inline void interface_entry_del (ubi_trNodePtr data, void *cookie) {
    interface_entry_t *entry = (interface_entry_t *) data;
    if (entry) {
        free (entry);
    }
}

#define INTERFACE_CACHE_MAX_ENTRIES 255
#define INTERFACE_CACHE_MAX_MEMORY 255 * 16

static void
nova_interface_cache (ubi_cacheRoot *root, module_t *module) {
    int32_t interfaceIndex = 0;
    DEBUGP (DDEBUG,"nova_interface_cache","caching interfaces for '%s'",module->name);
    while (module->implements[interfaceIndex]) {
        interface_entry_t *check = (interface_entry_t *)ubi_cacheGet (root, (void *)module->implements[interfaceIndex]);
        if (!check) {           /* prevent duplicate entry addition */
            interface_entry_t *entry = (interface_entry_t *)calloc (1,sizeof (interface_entry_t));
            if (entry) {
                entry->key = module->implements[interfaceIndex];
                entry->value = module->interfaces[interfaceIndex];
                DEBUGP (DDEBUG,"nova_interface_cache","putting '%s with %p' into cache",entry->key,entry->value);
                (void)ubi_cachePut (root, sizeof (interface_entry_t) + sizeof (interface_t), (ubi_cacheEntryPtr)entry, entry->key);
            }
        }
        interfaceIndex++;
    }
    if (module->requires) {
        int32_t moduleIndex = 0;
        while (module->requires[moduleIndex]) {
            nova_interface_cache (root, module->modules[moduleIndex]);
            moduleIndex++;
        }
    }
}

interface_t *
nova_interface_lookup (module_t *module, const char *name) {

    if (module && name && module->cache) {
        interface_entry_t *entry = NULL;
        MUTEX_LOCK (module->cache_lock);
        entry = (interface_entry_t *)ubi_cacheGet (module->cache, (void *)name);
        MUTEX_UNLOCK (module->cache_lock);
        if (entry) {
            //DEBUGP (DDEBUG,"nova_interface_lookup","found match for '%s' at %p",name,entry->value);
            return entry->value;
        } else {
            /* 
             * if (module->requires) {
             *     int32_t moduleIndex = 0;
             *     while (module->requires[moduleIndex]) {
             *         interface_t *match = nova_interface_lookup (module->modules[moduleIndex],name);
             *         if (match)
             *             return match;
             *         moduleIndex++;
             *     }
             * }
             */
        }
    }

    /*
     * this is a definite segfault...
     */
    DEBUGP (DDEBUG,"nova_interface_lookup","we are going to segfault now!");
    return NULL;
}

module_t *
nova_load(const char *name) {
    lt_dlhandle dll;
    nova_init();
    if ((dll = nova_dlopen(name))) {
        module_t *m = NULL;

#ifdef LTDL_OLD
        lt_dlloader * loader = dll->loader;
#else
        lt_dlvtable *loader = (lt_dlvtable *) lt_dlloader_get(dll);
#endif
        loader->sym_prefix = _extractPrefix(name);
        if (loader->sym_prefix) {
            DEBUGP(DDEBUG, "load", "looking for 'this' as %sxxx_LTX_this", loader->sym_prefix);
        }
        if ((m = (module_t *) lt_dlsym(dll, "this"))) {
            if (loader->sym_prefix) {
                free((char *) loader->sym_prefix);
                loader->sym_prefix = NULL;
            }
            /* dang this is hackish. :) */
            if (name) {
                if (!m->name)
                    m->name = strdup(name ? name : "Unknown");
            } else {
                m->name = strdup(m->name);
            }
            const char *thisName = name ? name : m->name;

            //DEBUGP (DDEBUG,"load","name: %s, m->name: %s",name,m->name);

            if (m->accessCounter) { /* already loaded! */
                DEBUGP(DDEBUG, "load", "%s - already loaded %u times.", thisName, m->accessCounter);
                /* 
                 * int32_t idx = 0;
                 * while (m->requires[idx]) {
                 *     char *moduleName = m->requires[idx];
                 *     nova_load (moduleName);
                 *     idx++;
                 * }
                 */
                LOADER_LOCK();
                _incrementAccess(m);
                /* m->accessCounter++; */
                LOADER_UNLOCK();
                lt_dlclose(dll);
                return m;
            } else { /* load new */
                m->dll = dll;
                m->accessCounter = 1;
                m->lock = (MUTEX_TYPE *) malloc(sizeof (MUTEX_TYPE));
                MUTEX_SETUP(m->lock[0]);

                m->modules = calloc(MODULE_REQUIRES_MAX + 1, sizeof (dependency_t));
                if (m->modules) {
                    int32_t idx = 0;
                    while (m->requires[idx]) {
                        char *moduleName = m->requires[idx];
                        DEBUGP(DDEBUG, "load", "%s - loading required module '%s'", thisName, moduleName);
                        if (idx < MODULE_REQUIRES_MAX) {
                            m->modules[idx] = nova_load(moduleName);
                            if (!m->modules[idx]) {
                                DEBUGP(DERROR, "load", "%s - cannot add dependency '%s'", thisName, moduleName);
                                nova_unload(m);
                                return NULL;
                            }
                        } else {
                            DEBUGP(DERROR, "load", "%s - too many required modules!", thisName);
                            nova_unload(m);
                            return NULL;
                        }
                        idx++;
                    }
                    if (idx) {
                        DEBUGP(DDEBUG, "load", "%s - added %u required module(s)", thisName, idx);
                    }
                } else {
                    DEBUGP(DERROR, "load", "%s - cannot allocate space for additional modules!", thisName);
                    nova_unload(m);
                    return NULL;
                }

                m->interfaces = calloc(MODULE_IMPLEMENTS_MAX + 1, sizeof (interface_t));
                if (m->interfaces) {
                    int32_t idx = 0;
                    while (m->implements[idx]) {
                        char *interfaceName = m->implements[idx];
                        DEBUGP(DDEBUG, "load", "%s - loading implemented interface '%s'", thisName, interfaceName);
                        if (idx < MODULE_IMPLEMENTS_MAX) {
                            m->interfaces[idx] = (interface_t) lt_dlsym(dll, interfaceName);
                            if (!m->interfaces[idx]) {
                                DEBUGP(DERROR, "load", "%s - cannot add interface '%s'", thisName, interfaceName);
                                nova_unload(m);
                                return NULL;
                            }
                        } else {
                            DEBUGP(DERR, "load", "%s - too many interfaces implemented!", thisName);
                            nova_unload(m);
                            return NULL;
                        }
                        idx++;
                    }
                    DEBUGP(DDEBUG, "load", "%s - added %u implemented interface(s)", thisName, idx);
                } else {
                    DEBUGP(DERROR, "load", "%s - cannot allocate space for interfaces!", thisName);
                    nova_unload(m);
                    return NULL;
                }

                /*
                 * setup the interface implementation cache
                 */ 
                if (m->implements) {
                    ubi_cacheRoot *root = (ubi_cacheRoot *)calloc (1,sizeof (ubi_cacheRoot));
                    if (root) {
                        MUTEX_SETUP(m->cache_lock);
                        m->cache = (void *) ubi_cacheInit (root,interface_entry_cmp,interface_entry_del,
                                                           INTERFACE_CACHE_MAX_ENTRIES,
                                                           INTERFACE_CACHE_MAX_MEMORY, NULL);
                        if (m->cache) {
                            nova_interface_cache (root, m);
                        }
                    }
                }
                
                DEBUGP(DDEBUG, "load", "%s - ready!", thisName);
                return m;
            }
        } else {


            DEBUGP(DERR, "load", "%s - is an incompatible module!", name);
            lt_dlclose(dll);
        }
    } else {
        DEBUGP(DERR, "load", "%s - cannot be found in search path (%s)", name, lt_dlgetsearchpath());
    }
    return NULL;
}

void
nova_unload(module_t *module) {

    static boolean_t shouldWait = TRUE;

    if (QuarkCount > 0 && shouldWait) {

        DEBUGP(DINFO, "nova_unload", "waiting 1sec for %u quark(s) to finish", QuarkCount);
        sleep(1);

        if (QuarkCount > 0)
            DEBUGP(DWARN, "nova_unload", "%u quark(s) still there", QuarkCount);

        shouldWait = FALSE;

    }

    if (module) {
        int32_t moduleIndex = 0;
        while (module->modules[moduleIndex]) {
            /* unload dependents first */
            nova_unload(module->modules[moduleIndex]);
            moduleIndex++;
        }
        LOADER_LOCK();
        module->accessCounter--;
        /*
         * DEBUGP(DINFO,"unload","removing reference to %s (%u left)",
         *        module->name,
         *        module->accessCounter);
         */
        if (!module->accessCounter && module->dll) {
            DEBUGP(DDEBUG, "unload", "freeing '%s' completely from memory footprint!", module->name);
            free((char *) module->name);
            free(module->modules);
            free(module->interfaces);

            if (module->cache) {
                ubi_cacheClear (module->cache);
                free (module->cache);
            }
            MUTEX_CLEANUP (module->cache_lock);
            
            MUTEX_CLEANUP(module->lock[0]);
            free(module->lock);
            lt_dlclose(module->dll);
            /*
              no need to free module since it is no longer accessible when you
              unload the dll!
             */
        }
        LOADER_UNLOCK();
    }
}

