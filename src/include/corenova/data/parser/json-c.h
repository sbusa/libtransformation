#ifndef __json-c_parser_H__
#define __json-c_parser_H__

#include <corenova/interface.h>
//#include "json.h"

typedef struct {


	// Define json-c structure members here
    MUTEX_TYPE lock;            /* makes operations thread-safe */
    
} jsonParser_t;



/*//////// MODULE DECLARATION //////////////////////////////////////////*/

DEFINE_INTERFACE (JsonParser)
{
	jsonParser_t*  (*new)     (char *, int length);	
	void      (*destroy) (jsonParser_t *);
	//int 	  (*getBufferLength) (jsonParser_t *);
    char *    (*toString) (jsonParser_t *);
    //int 	  (*removeLinks)(jsonParser_t *,array_t *val);
    //int       (*add)     (array_t *, void *item);
    //void      (*remove)  (array_t *, array_del_func del);
    //void     *(*get)     (array_t *, int index);
    //void     *(*first)   (array_t *);
    //void     *(*last)    (array_t *);
    //void     *(*replace) (array_t *, int index, void *with);
    //void     *(*clear)   (array_t *, int index);
    //void      (*cleanup) (array_t *);
    //int       (*compare) (array_t *, array_t *, array_cmp_func cmp);
    //int       (*find)    (array_t *, void *key, int start, array_cmp_func cmp);
    //void     *(*match)   (array_t *, void *key, array_cmp_func cmp);
    //boolean_t (*split)   (array_t *, int where, array_t **first, array_t **second);
    //int       (*merge)   (array_t **to, array_t **with);
    //array_t  *(*clone)   (const array_t *);
};

#endif
