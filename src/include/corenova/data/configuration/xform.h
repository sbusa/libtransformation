#ifndef __configuration_xform_H__
#define __configuration_xform_H__

#include <corenova/interface.h>

#include <corenova/data/configuration.h>

DEFINE_INTERFACE (XformConfigParser)
{
	configuration_t *(*parse)       (const char *filename);
	boolean_t        (*write)       (const char *file, configuration_t *config);
};

#endif
