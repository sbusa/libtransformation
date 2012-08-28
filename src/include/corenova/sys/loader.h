#ifndef __loader_H__
#define __loader_H__

#include <corenova/interface.h>
#include <corenova/module.h>

DEFINE_INTERFACE (DynamicLoader)
{
	module_t *(*load)          (const char *name);
	void      (*unload)        (module_t *module);
	void     *(*symbol)        (module_t *module, const char *name);
	/*
	  PATH can be a list of directories, dir1:dir2:dir3:etc...
	*/
	void      (*addSearchPath) (const char *path);
};

#endif
