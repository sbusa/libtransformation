#ifndef __configuration_ini_H__
#define __configuration_ini_H__

#include <corenova/interface.h>

#include <corenova/data/file.h>
#include <corenova/data/configuration.h>

DEFINE_INTERFACE (IniConfigParser)
{
	configuration_t *(*parse) (const char *string);
	configuration_t *(*parseByFile) (file_t *file);
	configuration_t *(*parseByFilename) (const char *filename);
	boolean_t        (*write) (const char *file, configuration_t *config);
};

#endif
