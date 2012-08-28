#ifndef __parameters_H__
#define __parameters_H__

#include <corenova/interface.h>

#include <corenova/data/string.h>
#include <corenova/data/md5.h>

typedef struct _param {
	char *key;
	char *val;
	struct _param *next;
} param_t;

typedef struct _parameters {
	param_t *first;
	param_t *last;
	uint32_t count;
} parameters_t;

/*//////// MODULE DECLARATION //////////////////////////////////////////*/

DEFINE_INTERFACE (Parameters)
{
	parameters_t  *(*new)      (void);
	void           (*destroy)  (parameters_t **);
	boolean_t      (*add)      (parameters_t *, const char *key, const char *value);
	boolean_t      (*update)   (parameters_t *, const char *key, const char *value);
	param_t       *(*get)      (parameters_t *, const char *key);
	char          *(*getValue) (parameters_t *, const char *key);
	char          *(*toString) (parameters_t *);
    parameters_t  *(*copy)     (parameters_t *);
    parameters_t  *(*join)     (parameters_t *one, parameters_t *two);
    md5_t         *(*md5)      (parameters_t *);
    int            (*getByteValue) (parameters_t *, const char *key);
    int            (*getTimeValue) (parameters_t *, const char *key);
    boolean_t      (*getBooleanValue) (parameters_t *, const char *key);
    int            (*getNumValue) (parameters_t *, const char *key);
};
#endif
