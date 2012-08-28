#include <corenova/source-stub.h>

THIS = {
	.version = "2.0",
	.author  = "Peter K. Lee <saint@corenova.com>",
	.description = "This module represents a category of information.",
	.implements = LIST ("Category"),
	.requires = LIST ("corenova.data.parameters")
};

#include <corenova/data/category.h>
#include <corenova/data/string.h>

/*//////// INTERNAL CODE //////////////////////////////////////////*/



/*//////// INTERFACE ROUTINES //////////////////////////////////////////*/

static category_t *
_new (const char *name) {
	category_t *c = (category_t *)calloc(1,sizeof(category_t));
	if (c) {
		c->name   = strdup(name);
		c->params = I (Parameters)->new();
		if (!c->params) {
			free(c); c = NULL;
		}
	}
	return c;
}

static void
_destroy (category_t **c) {
	if (c && *c) {
		if ((*c)->next) _destroy(&((*c)->next));
		I (Parameters)->destroy(&((*c)->params));
		free((*c)->name);
		free(*c);
		*c = NULL;
	}
}

static char *
_toString (category_t *c) {
	char *outString = NULL;
	while (c != NULL) {
		char *paramString = I (Parameters)->toString(c->params);
		I (String)->join(&outString,"[");
		I (String)->join(&outString,c->name);
		I (String)->join(&outString,"]\n");
		I (String)->join(&outString,paramString);
		I (String)->join(&outString,"\n");
		free(paramString);
		c = c->next;
	}
	return outString;
}

static boolean_t
_addDuplicate (category_t *c, category_t *dup) {
	if (c && dup) {
		category_t *last = c;
		while (last->next != NULL) last = last->next;
		last->next = dup;
		c->dupCount++;
		return TRUE;
	}
	return FALSE;
}

/*
  set of wrappers to Parameters interface
 */
static boolean_t
_setParameter (category_t *c, const char *key, const char *value) {
	return I(Parameters)->update(c->params,key,value);
}

static param_t *
_getParameter (category_t *c, const char *key) {
	return I(Parameters)->get(c->params,key);
}

static parameters_t *
_getCategoryParameters (category_t *c) {
    return c->params;
}

static char *
_getParamValue (category_t *c, const char *key) {
	return I(Parameters)->getValue(c->params,key);
}

static category_t *
_nextCategory (category_t *c) {
	return c->next;
}

static category_t *
_copyCategory (category_t *c) {
    if (c) {
        category_t *copy, *new;
        copy = new = (category_t *)calloc(1,sizeof(category_t));
        while (c != NULL && new != NULL) {
            new->name = strdup (c->name);
            new->params = I (Parameters)->copy (c->params);
            if (c->next) {
                new->next = (category_t *)calloc(1,sizeof(category_t));
                new = new->next;
            }
            c = c->next;
        }
        return copy;
    }
    return NULL;
}

IMPLEMENT_INTERFACE (Category) = {
	.new      = _new,
	.destroy  = _destroy,
	.addDuplicate  = _addDuplicate,
	.setParameter  = _setParameter,
	.getParameter  = _getParameter,
    .getParameters = _getCategoryParameters,
	.getParamValue = _getParamValue,
	.next          = _nextCategory,
    .copy          = _copyCategory,
	.toString      = _toString
};
