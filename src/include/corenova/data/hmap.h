#ifndef __hash_map_H__
#define __hash_map_H__

#include <corenova/interface.h>

#define DEFAULT_CAPACITY 1024
#define RESIZE_THRESHOLD 95

#define DEPTH_KEEPALL 0
#define DEPTH_KEEPDATA 1
#define DEPTH_KEEPKEYVAL 2
#define DEPTH_KEEPNONE 3

#ifndef OVERRIDE_STRING_COMPAR_FUNC

static int string_compar_func(const void *a, const void *b) {

    return strcmp(*(char**) a, *(char**) b);

}

#endif

typedef struct keyval {
    void *key;
    uint32_t keylen;
    void *val;
    uint32_t vallen;

} hkeyval_t;

#ifndef OVERRIDE_STRING_DUMP_FUNC

static void string_dump_func(hkeyval_t *a) {

    printf("[%s => %s ]", (char*) a->key, (char*) a->val);

}

#endif

typedef struct hash_node {
    hkeyval_t *data;
    struct hash_node *next;

} hnode_t;

typedef struct hash_map {
    uint32_t capacity;
    uint32_t occupancy;
    hnode_t **data;
    MUTEX_TYPE lock;

} hmap_t;

typedef struct hash_iter {
    int n;
    hmap_t *map;
    hnode_t *node;

} hiter_t;

DEFINE_INTERFACE(HKeyVal) {

    hkeyval_t * (*make) (char *, char *);
    hkeyval_t * (*make2) (void *, uint32_t, void *, uint32_t);
    hkeyval_t * (*key) (char *);
    hkeyval_t * (*key2) (void *, uint32_t);

};

DEFINE_INTERFACE(HMap) {
    hmap_t * (*new) (uint32_t size);
    int (*put) (hmap_t *, hkeyval_t *);
    int (*contains) (hmap_t *, hkeyval_t *);
    hkeyval_t * (*remove) (hmap_t *, hkeyval_t *);
    hkeyval_t * (*get) (hmap_t *, hkeyval_t*);
    hnode_t * (*first) (hiter_t *);
    hnode_t * (*next) (hiter_t *);
    uint32_t(*count) (hmap_t *);
    uint32_t(*size) (hmap_t *);
    void (*resize) (hmap_t *, uint32_t size);
    void (*destroy) (hmap_t **);
    void (*clear) (hmap_t *, int depth);
    void (*sdump) (hmap_t *);
    void (*dump) (hmap_t *, void (*dump_func)(hkeyval_t *));
    void **(*keyset) (hmap_t *);
    void **(*sortedkeyset) (hmap_t *, int (*compar)(const void *, const void *));
    char **(*ssortedkeyset) (hmap_t *);
    void **(*valset) (hmap_t *);
    hiter_t * (*iter) (hmap_t * map);

};

#endif
