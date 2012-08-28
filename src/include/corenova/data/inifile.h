#ifndef __inifile_H__
#define __inifile_H__

#include <corenova/interface.h>

#include <corenova/data/file.h>
#include <corenova/data/configuration.h>

DEFINE_INTERFACE (IniFileParser)
{
	configuration_t *(*parse) (const char *file);
	boolean_t        (*write) (const char *file, configuration_t *config);
};

#endif
