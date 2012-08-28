#ifndef __getopts_H__
#define __getopts_H__

#include <corenova/interface.h>

#include <getopt.h>

struct option StandardOpts[] = {
	{ "version", 0, NULL, 'V' },
	{ "daemon",  0, NULL, 'D' },
	{ "help",    0, NULL, 'h' },
	{ "logfile", 1, NULL, 'L' },
	{ 0, 0, 0, 0 }
};

#include <corenova/module.h>
#include <corenova/data/parameters.h>

DEFINE_INTERFACE (OptionParser)
{
	parameters_t *(*parse) (module_t *, int32_t argc, char **argv);
};

#endif
