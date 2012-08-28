#ifndef __category_H__
#define __category_H__

#include <corenova/interface.h>

#include <corenova/data/parameters.h>

typedef struct _category {
	char *name;
	parameters_t *params;
	uint32_t dupCount;
	struct _category *next; 	/* if there are duplicates */
} category_t;

/*//////// MODULE DECLARATION //////////////////////////////////////////*/

DEFINE_INTERFACE (Category)
{
	category_t   *(*new)           (const char *name);
	void          (*destroy)       (category_t **);
	boolean_t     (*addDuplicate)  (category_t *, category_t *dup);
	boolean_t     (*setParameter)  (category_t *, const char *key, const char *value);
	param_t      *(*getParameter)  (category_t *, const char *key);
    parameters_t *(*getParameters) (category_t *);
	char         *(*getParamValue) (category_t *, const char *key);
	category_t   *(*next)          (category_t *);
    category_t   *(*copy)          (category_t *);
	char         *(*toString)      (category_t *);
};

#endif
