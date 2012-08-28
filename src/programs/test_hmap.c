#include <corenova/source-stub.h>

THIS = {
    .name = "Test HMap",
    .version = "0.0",
    .author = "Alex Burkoff <alex.burkoff@gmail.com>",
    .description = "Demonstrate HMap functionality.",
    .requires = LIST("corenova.data.hmap")
};

#include <corenova/sys/getopts.h>
#include <corenova/sys/debug.h>

#define OVERRIDE_STRING_COMPAR_FUNC
#define OVERRIDE_STRING_DUMP_FUNC
#include <corenova/data/hmap.h>

static int string_compar_func(const void *a, const void *b) {

    return strcmp(*(char**) a, *(char**) b)*-1;

}

static void string_dump_func(hkeyval_t *a) {

    DEBUGP(DDEBUG, "main", "[%s => %s ]", (char*) a->key, (char*) a->val);

}

int main(int argc, char **argv, char **envp) {

    DebugLevel = 6;

    hmap_t *map = I(HMap)->new(10);

    I(HMap)->put(map, I(HKeyVal)->make(strdup("sux"), strdup("suxer")));
    I(HMap)->put(map, I(HKeyVal)->make(strdup("hello"), strdup("world")));

    char c = 'a';

    for (; c <= 'z'; c++) {

        char skey[] = { "key x"};
        char sval[] = { "val x"};

        skey[4] = c; sval[4] = c;

        I(HMap)->put(map, I(HKeyVal)->make(strdup(skey), strdup(sval)));

    }

    I(HMap)->sdump(map);

    hkeyval_t *key = I(HKeyVal)->key("key v");
    
    key = I(HMap)->get(map,key);
    
    DEBUGP(DDEBUG, "main", "key v => %s (should be 'val v')", key != NULL ? (char*) key->val : NULL);

    I(HMap)->remove(map, key);

    key = I(HMap)->get(map, key);

    DEBUGP(DDEBUG, "main", "key v => %s (should be null)", key != NULL ? (char*) key->val : NULL);

    I(HMap)->sdump(map);

    hiter_t *iter = I(HMap)->iter(map);
    hnode_t *n;

    for (n = I(HMap)->first(iter); n; n = I(HMap)->next(iter)) {

        string_dump_func(n->data);

    }

    char **keys = (char**) I(HMap)->sortedkeyset(map, string_compar_func);
    char **vals = (char**) I(HMap)->valset(map);

    int i = 0;
    
    DEBUGP(DDEBUG, "main", "keys in reverse-sorted order:");

    for (i = 0; i < map->occupancy; i++) {

        DEBUGP(DDEBUG, "main", "%u => %s", i, keys[i]);

    }
    
    DEBUGP(DDEBUG, "main", "values in unsorted order:");

    for (i = 0; i < map->occupancy; i++) {

        DEBUGP(DDEBUG, "main", "%u => %s", i, vals[i]);

    }

    DEBUGP(DDEBUG, "main", "values in reverse-sorted order:");

    for (i = 0; i < map->occupancy; i++) {
        
        key = I(HKeyVal)->key(keys[i]);

        DEBUGP(DDEBUG, "main", "%u => %s", i, (char*)I(HMap)->get(map, key)->val);

    }
    
    I(HMap)->clear(map, DEPTH_KEEPNONE);
    I(HMap)->destroy(&map);

    return 0;

}

