#include <corenova/source-stub.h>

THIS = {
    .version = "2.0",
    .author = "Peter K. Lee <saint@corenova.com>",
    .description = "Hash Map.",
    .implements = LIST("HMap", "HKeyVal")
};

#include <corenova/data/hmap.h>

static void _resize(hmap_t *map, int newsize);

static hiter_t *_iter(hmap_t *map) {

    hiter_t *iter = malloc(sizeof (hiter_t));

    iter->map = map;
    iter->n = 0;
    iter->node = map->data[0];

    return iter;

}

static hnode_t *_first(hiter_t *iter) {
    
    MUTEX_LOCK (iter->map->lock);

    while (iter->n < iter->map->capacity) {

        if (iter->node == NULL) {

            iter->n++;
            iter->node = iter->map->data[iter->n];

        }

        if (iter->node != NULL) {
            
            MUTEX_UNLOCK (iter->map->lock);
            return iter->node;
            
        }

    }
    
    MUTEX_UNLOCK (iter->map->lock);

    return NULL;

}

static hnode_t *_next(hiter_t *iter) {
    
    MUTEX_LOCK (iter->map->lock);

    while (iter->n < iter->map->capacity) {

        if (iter->node == NULL) {

            iter->n++;
            iter->node = iter->map->data[iter->n];

        } else {

            iter->node = iter->node->next;

        }

        if (iter->node != NULL) {
            
            MUTEX_UNLOCK (iter->map->lock);
            return iter->node;
            
        }


    }
    
    MUTEX_UNLOCK (iter->map->lock);

    return NULL;

}

unsigned hash_func(void *key, int len) {
    unsigned char *p = key;
    unsigned h = 0;
    int i;
    for (i = 0; i < len; i++)
        h = 33 * h ^ p[i];
    return h;
}

static int _contains(hmap_t *map, hkeyval_t *keyval) {

    MUTEX_LOCK (map->lock);
    
    unsigned index = hash_func(keyval->key, keyval->keylen) % map->capacity;

    hnode_t *it = map->data[index];

    while (it != NULL && (it->data->keylen != keyval->keylen || memcmp(it->data->key, keyval->key, keyval->keylen) != 0))
        it = it->next;
    
    MUTEX_UNLOCK (map->lock);

    return it != NULL;

}

static hkeyval_t *_get(hmap_t *map, hkeyval_t *keyval) {
    
    MUTEX_LOCK (map->lock);

    unsigned index = hash_func(keyval->key, keyval->keylen) % map->capacity;

    hnode_t *it = map->data[index];

    while (it != NULL && (it->data->keylen != keyval->keylen || memcmp(it->data->key, keyval->key, keyval->keylen) != 0))
        it = it->next;

    MUTEX_UNLOCK (map->lock);
    
    return it != NULL ? it->data : NULL;

}

static hkeyval_t *_remove(hmap_t *map, hkeyval_t *keyval) {
    
    MUTEX_LOCK (map->lock);

    unsigned index = hash_func(keyval->key, keyval->keylen) % map->capacity;

    hnode_t *it = map->data[index], *prev = NULL;

    while (it != NULL && (it->data->keylen != keyval->keylen || memcmp(it->data->key, keyval->key, keyval->keylen) != 0)) {

        prev = it;
        it = it->next;

    }

    if (it != NULL) {

        if (prev != NULL) {

            prev->next = it->next;

        } else {

            map->data[index] = it->next;

        }

        map->occupancy--;

    }
    
    MUTEX_UNLOCK (map->lock);

    return it != NULL ? it->data : NULL;

}

static int _put(hmap_t *map, hkeyval_t *keyval) {

    if (!_contains(map, keyval)) {
        
        MUTEX_LOCK (map->lock);

        if ((map->occupancy + 1) * 100 / map->capacity >= RESIZE_THRESHOLD) {

            _resize(map, map->capacity * 2);

        }

        unsigned index = hash_func(keyval->key, keyval->keylen) % map->capacity;

        hnode_t *new_node = malloc(sizeof *new_node);

        if (new_node == NULL) {
            
            MUTEX_UNLOCK (map->lock);
            return 0;
            
        }

        new_node->data = keyval;
        new_node->next = map->data[index];

        map->data[index] = new_node;
        map->occupancy++;
        
        MUTEX_UNLOCK (map->lock);

        return 1;

    }

    return 0;

}

static hmap_t* _new(uint32_t capacity) {

    hmap_t *result = calloc(1, sizeof (hmap_t));

    result->capacity = capacity;
    result->data = calloc(1, sizeof (hnode_t*) * capacity);

    MUTEX_SETUP(result->lock);

    return result;

}

static void _destroy(hmap_t **map) {

    hmap_t *p = *map;
    
    MUTEX_CLEANUP(p->lock);
    
    free(p->data);
    free(p);

    p = NULL;

}

static void _resize(hmap_t *map, int newsize) {
    
    MUTEX_LOCK (map->lock);

    hnode_t **oldtab = map->data;
    int oldcap = map->capacity;

    map->capacity = newsize;
    map->occupancy = 0;
    map->data = calloc(1, sizeof (hnode_t*) * newsize);

    int i = 0;

    for (i = 0; i < oldcap; i++) {

        if (oldtab[i] != NULL) {

            hnode_t *it = oldtab[i];

            while (it != NULL) {

                _put(map, it->data);

                it = it->next;

            }

        }

    }

    free(oldtab);
    
    MUTEX_UNLOCK (map->lock);

}

static void _clear(hmap_t *map, int depth) {

    int i = 0;
    
    MUTEX_LOCK (map->lock);

    for (i = 0; i < map->capacity; i++) {

        if (map->data[i] != NULL) {

            hnode_t *it = map->data[i], *prev = NULL;

            while (it != NULL) {

                prev = it;
                it = it->next;

                switch (depth) {

                    case DEPTH_KEEPALL:
                        break;

                    case DEPTH_KEEPKEYVAL:
                        free(prev->data);
                        free(prev);
                        break;
                    case DEPTH_KEEPDATA:
                        free(prev);
                        break;
                    case DEPTH_KEEPNONE:
                        free(prev->data->key);
                        free(prev->data->val);
                        free(prev->data);
                        free(prev);
                        break;

                }

            }

        }

    }
    
    MUTEX_UNLOCK (map->lock);

}

static void _dump(hmap_t *map, void (*dump_func)(hkeyval_t*)) {
    
    MUTEX_LOCK (map->lock);

    printf("dumping map %0x with capacity %u and occupancy %u\r\n", (int) map, map->capacity, map->occupancy);

    int i = 0;

    for (i = 0; i < map->capacity; i++) {

        if (map->data[i] != NULL) {

            printf("%u => ", i);

            hnode_t *it = map->data[i];

            while (it != NULL) {

                dump_func(it->data);

                it = it->next;

                if (it != NULL) {

                    printf(",");

                }

            }

            printf("\r\n");

        }

    }
    
    MUTEX_UNLOCK (map->lock);

}

static void** _keyset(hmap_t *map) {

    hiter_t *iter = _iter(map);

    hnode_t *n;
    int i;
    void **result = malloc(map->occupancy * sizeof (void*));

    for (n = _first(iter), i = 0; n; n = _next(iter), i++) {

        result[i] = n->data->key;

    }

    free(iter);

    return result;

}

static void** _sortedkeyset(hmap_t *map, int (*compar)(const void *, const void *)) {

    void **result = _keyset(map);

    qsort(result, map->occupancy, sizeof (void *), compar);

    return result;

}

static void **_valset(hmap_t *map) {

    hiter_t *iter = _iter(map);

    hnode_t *n;
    int i;
    void **result = malloc(map->occupancy * sizeof (void*));

    for (n = _first(iter), i = 0; n; n = _next(iter), i++) {

        result[i] = n->data->val;

    }

    free(iter);

    return result;

}

static char **_ssortedkeyset(hmap_t *map) {

    char **keys = (char**) _sortedkeyset(map, string_compar_func);

    return keys;

}

static void _sdump(hmap_t *map) {

    _dump(map, string_dump_func);

}

static hkeyval_t *make_keyval(char *key, char *val) {

    hkeyval_t *keyval = malloc(sizeof (hkeyval_t));

    keyval->key = key;
    keyval->keylen = strlen(key) + 1;
    keyval->val = val;
    keyval->vallen = strlen(val) + 1;

    return keyval;

}

static hkeyval_t *make_keyval2(void *key, uint32_t keylen, void *val, uint32_t vallen) {

    hkeyval_t *keyval = malloc(sizeof (hkeyval_t));

    keyval->key = key;
    keyval->keylen = keylen;
    keyval->val = val;
    keyval->vallen = vallen;

    return keyval;

}

static hkeyval_t *make_key(char *key) {

    hkeyval_t *keyval = malloc(sizeof (hkeyval_t));

    keyval->key = key;
    keyval->keylen = strlen(key) + 1;

    return keyval;

}

static hkeyval_t *make_key2(void *key, uint32_t keylen) {

    hkeyval_t *keyval = malloc(sizeof (hkeyval_t));

    keyval->key = key;
    keyval->keylen = keylen;

    return keyval;

}

static uint32_t _count(hmap_t *map) {

    return map->occupancy;

}

static uint32_t _size(hmap_t *map) {

    return map->capacity;

}

IMPLEMENT_INTERFACE(HKeyVal) = {
    .make = make_keyval,
    .make2 = make_keyval2,
    .key = make_key,
    .key2 = make_key2
};

IMPLEMENT_INTERFACE(HMap) = {
    .new = _new,
    .put = _put,
    .contains = _contains,
    .remove = _remove,
    .get = _get,
    .count = _count,
    .size = _size,
    .next = _next,
    .first = _first,
    .destroy = _destroy,
    .keyset = _keyset,
    .sortedkeyset = _sortedkeyset,
    .valset = _valset,
    .clear = _clear,
    .iter = _iter,
    .sdump = _sdump,
    .dump = _dump,
    .ssortedkeyset = _ssortedkeyset
};


