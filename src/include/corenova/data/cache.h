#ifndef __corenova_data_cache_H__
#define __corenova_data_cache_H__

#include <corenova/interface.h>

#define CACHE_DEFAULT_MAXENTRIES 1000
#define CACHE_DEFAULT_MAXMEMORY  100000 /* 100KB */

/** call back comparison function (=(1),!=(0)) **/
typedef int (*cache_cmp_func) (void *first, void *second);

/** call back item deletion function **/
typedef void (*cache_del_func) (void *data, void *cookie);

/** call back item dump function **/
typedef void (*cache_dump_func) (void *data, void *cookie);

typedef struct {
    
    void *root;
    cache_cmp_func cmp;
    cache_del_func del;
    MUTEX_TYPE lock;            /* makes operations thread-safe */
    
} cache_t;

typedef struct {
    cache_dump_func  cb;
    void             *cookie;
} cache_dump_t;

/*//////// MODULE DECLARATION //////////////////////////////////////////*/

DEFINE_INTERFACE (Cache)
{
    cache_t  *(*new)     (cache_cmp_func cmp, cache_del_func del, uint32_t max_entries, uint32_t max_memory, void *cookie);
    void      (*put)     (cache_t *, void *key, void *data, uint32_t dataSize);
    void     *(*get)     (cache_t *, void *key);
    boolean_t *(*delete)  (cache_t *, void *key);
    void      (*dump)  (cache_t **, cache_dump_t *dump);
    void      (*destroy) (cache_t **);
};

#endif
