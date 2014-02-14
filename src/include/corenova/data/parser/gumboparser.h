#ifndef __gumbo_parser_H__
#define __gumbo_parser_H__

#include <corenova/interface.h>
#include "gumbo.h"

typedef struct {
	
	int  buffer_lenth;
	char *buffer;
	GumboOutput* output;
    MUTEX_TYPE lock;            /* makes operations thread-safe */
    
} gumboParser_t;

typedef enum 
{
	GUMBOPARSER_TAG_A=40,	
}GumboParserTag;;


/*//////// MODULE DECLARATION //////////////////////////////////////////*/

DEFINE_INTERFACE (GumboParser)
{
	gumboParser_t*  (*new)     (char *, int length);	
	void      (*destroy) (gumboParser_t *);
	//int 	  (*getBufferLength) (gumboParser_t *);
    char *    (*toString) (gumboParser_t *);
    //int 	  (*removeLinks)(gumboParser_t *,array_t *val);
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
