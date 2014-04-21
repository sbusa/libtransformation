#include <corenova/source-stub.h>

THIS = {
	.version     = "1.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "Provide in-memory data caching and lookup functionality.",
	.implements  = LIST ("Cache")
};

#include <corenova/data/cache.h>

// use LGPL open-source implementation of "splay tree" cache
#include <ubiqx/library/ubi_Cache.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

typedef ubi_cacheRoot cache_root_t;

typedef struct {
    ubi_cacheEntry node;
    cache_cmp_func cmp;
    cache_del_func del;
    void *data;
} __cache_entry_t;

static inline int
__cache_cmp_func ( ubi_trItemPtr itemPtr, ubi_trNodePtr nodePtr) {
    return ((__cache_entry_t *)nodePtr)->cmp (itemPtr,((__cache_entry_t *)nodePtr)->data);
}

static inline void
__cache_del_func (ubi_trNodePtr nodePtr, void *cookie) {
    ((__cache_entry_t *)nodePtr)->del (((__cache_entry_t *)nodePtr)->data, cookie);
    free ( nodePtr );
}

static cache_t *
newCache (cache_cmp_func cmp, cache_del_func del, uint32_t max_entries, uint32_t max_memory, void *cookie) {
    cache_t *cache = NULL;
    if (cmp && del) {
        if (!max_entries) max_entries = CACHE_DEFAULT_MAXENTRIES;
        if (!max_memory)  max_memory = CACHE_DEFAULT_MAXMEMORY;
        
        if ((cache = (cache_t *)calloc (1,sizeof (cache_t)))) {
            cache_root_t *root = (cache_root_t *)calloc (1,sizeof (cache_root_t));
            if (root) {
                cache->root = (void *) ubi_cacheInit (root,__cache_cmp_func,__cache_del_func,max_entries,max_memory,cookie);
                if (cache->root && cmp && del) {
                    cache->cmp = cmp;
                    cache->del = del;
                    MUTEX_SETUP (cache->lock);
                    DEBUGP (DINFO,"newCache","Cache created with MaxEntries: %lu and MaxMemory: %lu",
                            ubi_cacheGetMaxEntries (cache->root), ubi_cacheGetMaxMemory (cache->root));
                } else {
                    free (root);
                }
            } else {
                free (cache);
            }
        }
    }
    return cache;
}

static void
putInCache (cache_t *cache, void *key, void *data, uint32_t dataSize) {
    if (cache && key && data && dataSize) {
        __cache_entry_t *entry = (__cache_entry_t *)calloc (1,sizeof (__cache_entry_t));
        if (entry) {
            entry->cmp  = cache->cmp;
            entry->del  = cache->del;
            entry->data = data;
            MUTEX_LOCK (cache->lock);
            (void)ubi_cachePut ( cache->root, sizeof (__cache_entry_t) + dataSize, (ubi_cacheEntryPtr)entry, key);
            MUTEX_UNLOCK (cache->lock);
        }
    }
}

static void *
getFromCache (cache_t *cache, void * key) {
    if (cache && key) {
        __cache_entry_t *entry = NULL;
        MUTEX_LOCK (cache->lock);
        entry = (__cache_entry_t *)ubi_cacheGet ( cache->root, key );
        MUTEX_UNLOCK (cache->lock);
        if (entry) {
            DEBUGP (DDEBUG,"getCache","Cache Hit!");
            return entry->data;
        }
    }
    return NULL;
}

static boolean_t *
deleteFromCache (cache_t *cache, void *key) {
	if (cache && key ) {
		__cache_entry_t *entry = getFromCache(cache, key);
		if (entry) {
			MUTEX_LOCK (cache->lock);
			boolean_t ret = ubi_cacheDelete( cache->root, entry); 
        		MUTEX_UNLOCK (cache->lock);
			if (ret) {
				DEBUGP (DDEBUG, "deleteFromCache", "Deleted entry from Cache");
			}
			return ret;
		}
	}
	return FALSE;
}

static void
destroyCache (cache_t **cPtr) {
    if (cPtr && *cPtr) {
        cache_t *cache = *cPtr;
        if (cache->root) {
            cache_root_t *root = (cache_root_t *)cache->root;
            int hitRatio = ubi_cacheHitRatio (root);
            DEBUGP (DINFO,"destroyCache","Cache Hit Rate: %d.%02d%%",hitRatio/100,hitRatio%100);
            ubi_cacheClear (root);
	    if (root->cookie) free(root->cookie);
            free (cache->root);
            cache->root = NULL;
        }
        MUTEX_CLEANUP (cache->lock);
        free (cache);
        *cPtr = NULL;
    }
}

IMPLEMENT_INTERFACE (Cache) = {
	.new     = newCache,
    .put     = putInCache,
    .get     = getFromCache,
    .delete  = deleteFromCache,
    .destroy = destroyCache
};
