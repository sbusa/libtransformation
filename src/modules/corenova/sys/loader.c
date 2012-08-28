#include <corenova/source-stub.h>

THIS = {
	.version = "2.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "This module enables dynamic module loading for other modules.",
	.implements  = LIST ("DynamicLoader"),
	.requires    = LIST ("corenova.data.string")
};

#include <corenova/sys/loader.h>
#include <corenova/data/string.h>

/* refer to the NovaLoader library routines! */
extern module_t *nova_load   (const char *name);
extern void      nova_unload (module_t *module);
extern void     *nova_symbol (module_t *module, const char *sym);

static void
_addSearchDirectory (const char *dir) {
	boolean_t foundDirectory = FALSE;
	const char *searchPath = lt_dlgetsearchpath ();
	if (searchPath) {
		list_t *dirs = I (String)->tokenize (searchPath,":");
		if (dirs) {
			list_item_t *item = NULL;
			while ((item = I (List)->pop (dirs))) {
				char *dirEntry = (char *) item->data;
				if (I (String)->equal (dir,dirEntry)) {
					foundDirectory = TRUE;
				}
				free (dirEntry);
				free (item);
			}
			free (dirs);
		}
	}
	if (!foundDirectory)
		lt_dladdsearchdir (dir);
}

static void
addSearchPath (const char *path) {
	list_t *dirs = I (String)->tokenize (path,":");
	if (dirs) {
		list_item_t *item = NULL;
		while ((item = I (List)->pop (dirs))) {
			char *dirEntry = (char *) item->data;
			_addSearchDirectory (dirEntry);
			free (dirEntry);
			free (item);
		}
		free (dirs);
	}
}

static module_t *
dynamicLoad (const char *name) {
	if (name)
		return nova_load (name);
	return NULL;
}

IMPLEMENT_INTERFACE (DynamicLoader) = {
	.load          = dynamicLoad,
	.unload        = nova_unload,
	.symbol        = nova_symbol,
	.addSearchPath = addSearchPath
};
