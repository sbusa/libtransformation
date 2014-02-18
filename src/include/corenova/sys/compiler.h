#ifndef __tcc_H__
#define __tcc_H__

#include <corenova/interface.h>
#include <corenova/data/file.h>

#include <libtcc.h>

typedef struct {

    TCCState *s;
    
} tinycc_t;


DEFINE_INTERFACE (DynamicCompiler)
{
	tinycc_t *(*new)        (const char *opts);
	void      (*destroy)    (tinycc_t **);
    file_t   *(*preprocess) (tinycc_t *, const char *filename);

    /* in the future...
     * void (*compile) (tinycc_t *, array_t *files);
     */
};

#endif
