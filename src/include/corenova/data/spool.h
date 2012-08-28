#ifndef __spool_H__
#define __spool_H__

#include <corenova/interface.h>

#include <corenova/data/file.h>

typedef struct {
    
	char *name;
	char *current;

	boolean_t truncate;
	
	file_t *file;
	file_t *waldo;
    
} spool_t;

/*//////// MODULE DECLARATION //////////////////////////////////////////*/

/*
  put together the function interface available for export so that other
  modules can call them!
*/
DEFINE_INTERFACE (Spool) {
	spool_t  *(*new)          (const char *name, boolean_t enableTruncate);
	void      (*destroy)      (spool_t **);
	boolean_t (*read)         (spool_t *, void **buf, u_int32_t size, u_int16_t count);
	char     *(*getline)      (spool_t *, boolean_t multiline);
	boolean_t (*useWaldoFile) (spool_t *, const char *name);
};

#endif

