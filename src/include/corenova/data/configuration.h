#ifndef __configuration_H__
#define __configuration_H__

#include <corenova/interface.h>

#include <corenova/data/category.h>

typedef struct _category_list {
	category_t *category;
	struct _category_list *prev;
	struct _category_list *next;
} category_list_t;

typedef struct {
	char *name;
	category_list_t *categories;
	uint32_t categoryCount;
} configuration_t;

/*//////// MODULE DECLARATION //////////////////////////////////////////*/

DEFINE_INTERFACE (Configuration)
{
	configuration_t *(*new)         (const char *name);
	void             (*destroy)     (configuration_t **);
	category_t      *(*addCategory) (configuration_t *, const char *categoryName);
	boolean_t        (*delCategory) (configuration_t *, const char *categoryName);
	category_t      *(*getCategory) (configuration_t *, const char *categoryName);
    configuration_t *(*copy)        (configuration_t *);
	char            *(*toString)    (configuration_t *);
};

#endif
