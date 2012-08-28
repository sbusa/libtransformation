#ifndef __array_H__
#define __array_H__

#include <corenova/interface.h>

/** call back comparison function (=(1),!=(0)) **/
typedef int (*array_cmp_func) (void *first, void *second);

/** call back item deletion function **/
typedef void (*array_del_func) (void *data);

typedef struct {
    
	int    num;
	void **items;
    MUTEX_TYPE lock;            /* makes operations thread-safe */
    
} array_t;

/*//////// MODULE DECLARATION //////////////////////////////////////////*/

DEFINE_INTERFACE (Array)
{
	array_t  *(*new)     (void);
	void      (*destroy) (array_t **, array_del_func del);
    int       (*count)   (array_t *);
    int       (*add)     (array_t *, void *item);
    void      (*remove)  (array_t *, array_del_func del);
    void     *(*get)     (array_t *, int index);
    void     *(*first)   (array_t *);
    void     *(*last)    (array_t *);
    void     *(*replace) (array_t *, int index, void *with);
    void     *(*clear)   (array_t *, int index);
    void      (*cleanup) (array_t *);
    int       (*compare) (array_t *, array_t *, array_cmp_func cmp);
    int       (*find)    (array_t *, void *key, int start, array_cmp_func cmp);
    void     *(*match)   (array_t *, void *key, array_cmp_func cmp);
    boolean_t (*split)   (array_t *, int where, array_t **first, array_t **second);
    int       (*merge)   (array_t **to, array_t **with);
    array_t  *(*clone)   (const array_t *);
};

#endif
