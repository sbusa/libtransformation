#include <corenova/source-stub.h>

THIS = {
	.name = "Module Checker",
	.version = "1.0",
	.author = "Peter K. Lee <saint@corenova.com>",
	.description = "This program checks a module and retrieves information.",
	.requires = LIST ("corenova.sys.getopts",
					  "corenova.sys.loader"),
	.options = {
		OPTION ("name", "string", "name of module to check"),
		OPTION_NULL
	}
};

#include <corenova/sys/getopts.h>
#include <corenova/sys/loader.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

int32_t main(int32_t argc, char **argv, char **envp) {
	/* process command line arguments */
	parameters_t *params = I (OptionParser)->parse(&this,argc,argv);
	if (params && params->count) {
		char *moduleName = I (Parameters)->getValue (params,"name");
		module_t *module =  I (DynamicLoader)->load (moduleName);
		if (module) {
			printf ("\n"
					"name    = %s\n"
					"version = %s\n"
					"author  = %s\n"
					"\n%s\n\n",
					module->name,
					module->version,
					module->author,
					module->description);

			if (module->implements) {
				int32_t idx = 0;
				printf ("Interface Implementations:\n\n");
				while (module->implements[idx]) {
					printf ("\t%s\n",module->implements[idx]);
					idx++;
				}
				printf ("\n");
			}

			if (module->requires) {
				int32_t idx = 0;
				printf ("Required Dependencies:\n\n");
				while (module->requires[idx]) {
					printf ("\t%s\n",module->requires[idx]);
					idx++;
				}
				printf ("\n");
			}

            if (module->transforms) {
                int32_t idx = 0;
                printf ("Supported Transformations:\n\n");
                while (module->transforms[idx]) {
                    printf ("\t%s\n",module->transforms[idx]);
                    idx++;
                }
                printf ("\n");
            }
            
			I (DynamicLoader)->unload (module);
			DEBUGP (DINFO,"main","module successfully unloaded!");
		} else {
			DEBUGP (DERR,"main","cannot load requested module!");
		}
	} else {
		DEBUGP (DERR,"main","no parameters?");
	}
	return 0;
}

