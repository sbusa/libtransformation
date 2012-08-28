#include <corenova/source-stub.h>

THIS = {
	.version = "2.0",
	.author  = "Peter K. Lee <saint@corenova.com>",
	.description = "This module holds collection of categories with parameters.",
	.requires = LIST ("corenova.data.category"),
	.implements = LIST ("Configuration")
};

#include <corenova/data/configuration.h>

/*//////// INTERNAL CODE //////////////////////////////////////////*/

static category_list_t *
_getCategoryMatch (category_list_t *list, const char *name) {
	size_t len = strlen(name);
	while (list != NULL) {
		category_t *category = list->category;
		size_t cLen = strlen(category->name);
		if ((len == cLen) && !strncasecmp(category->name,name,len))
			return list;
		list = list->next;
	}
	return NULL;
}

static void
_destroyCategoryList (category_list_t *list) {
	if (list) {
		if (list->next) _destroyCategoryList(list->next);
		I (Category)->destroy(&list->category);
		free (list);
	}
}

static boolean_t
_addNewCategory (configuration_t *conf, category_t *newCategory) {
	category_list_t *newListItem = (category_list_t *)calloc(1,sizeof(category_list_t));
	if (newListItem) {
		newListItem->category = newCategory;
		if (conf->categories) {
			category_list_t *item = conf->categories;
			while (item->next != NULL) item = item->next;
			item->next = newListItem;
			newListItem->prev = item;
		} else {
			conf->categories = newListItem;
		}
		conf->categoryCount++;
		return TRUE;
	}
	return FALSE;
}

/*//////// INTERFACE ROUTINES //////////////////////////////////////////*/

static configuration_t *
_new (const char *name) {
	configuration_t *c = (configuration_t *)calloc(1,sizeof(configuration_t));
	if (c) {
		c->name = strdup(name);
	}
	return c;
}

static void
_destroy (configuration_t **conf) {
	if (conf && *conf) {
		_destroyCategoryList((*conf)->categories);
		free ((*conf)->name); free (*conf);
		*conf = NULL;
	}
}

static char *
_toString (configuration_t *conf) {
	I_TYPE(Category) *I_CATEGORY = I (Category);
	I_TYPE(String)   *I_STRING   = I (String);
	if (conf) {
		char *outString = NULL;
		category_list_t *item = conf->categories;
		while (item != NULL) {
			char *categoryString = I_CATEGORY->toString(item->category);
			I_STRING->join(&outString,categoryString);
			free(categoryString);
			item = item->next;
		}
		return outString;
	}
	return NULL;
}

/* 
 * static boolean_t
 * _mergeCategory (configuration_t *conf, category_t *category) {
 * 	if (conf && category) {
 * 		category_t *existingCategory = _getCategory(conf,category->name);
 * 		if (existingCategory) {
 * 			return I (Category)->addDuplicate(existingCategory,category);
 * 		} else {
 * 			return _addNewCategory(conf,category);
 * 		}
 * 	}
 * 	return FALSE;
 * }
 */

static category_t *
_getCategory (configuration_t *conf, const char *categoryName) {
	if (conf) {
		category_list_t *match = _getCategoryMatch(conf->categories,categoryName);
		return match?match->category:NULL;
	}
	return NULL;
}

static category_t *
_addCategory (configuration_t *conf, const char *categoryName) {
	I_TYPE (Category) *I_CATEGORY = I (Category);
	category_t *newCategory = I_CATEGORY->new(categoryName);
	if (newCategory) {
		category_t *oldCategory = _getCategory(conf,categoryName);
		if (oldCategory) {
			if (!I_CATEGORY->addDuplicate(oldCategory,newCategory)) {
				I_CATEGORY->destroy(&newCategory);
			}
		} else {
			if (!_addNewCategory(conf,newCategory)) {
				I_CATEGORY->destroy(&newCategory);
			}
		}
	}
	return newCategory;
}

static boolean_t
_delCategory(configuration_t *conf, const char *categoryName) {
	if (conf) {
		category_list_t *match = _getCategoryMatch(conf->categories,categoryName);
		if (match) {
			I (Category)->destroy(&match->category);
			if (match->prev)
				match->prev->next = match->next;
			else
				conf->categories = match->next;
			free (match);
			return TRUE;
		}
	}
	return FALSE;
}

static configuration_t *
cloneConfiguration (configuration_t *conf){
    if (conf) {
        configuration_t *copy = I (Configuration)->new (conf->name);
        if (copy) {
            category_list_t *item = conf->categories;
            while (item != NULL) {
                _addNewCategory (copy, I (Category)->copy (item->category));
                item = item->next;
            }
        }
        return copy;
    }
    return NULL;
}

IMPLEMENT_INTERFACE (Configuration) = {
	.new      = _new,
	.destroy  = _destroy,
	.addCategory = _addCategory,
	.delCategory = _delCategory,
	.getCategory = _getCategory,
    .copy        = cloneConfiguration,
	.toString    = _toString
};
