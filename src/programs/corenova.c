#include <corenova/source-stub.h>

THIS = {
	.name = "corenova",
	.version = PACKAGE_VERSION,
	.author = "Peter K. Lee <peter@corenova.com>",
	.description = "This program will create a new universe",
	.requires = LIST ("corenova.data.configuration.xform",
                      "corenova.data.array",
                      "corenova.sys.debug",
                      "corenova.sys.getopts",
                      "corenova.sys.signals",
                      "corenova.sys.transform"),
	.options = {
		OPTION ("id", "string", "an optional unique identifier representing the running instance"),
		OPTION ("engine", "filename", "location of engine paramters file"),
		OPTION ("transform", "filename", "location of transformation xfc file"),
		OPTION ("logdir", "directory", "log directory to write debug outputs"),
        OPTION ("verbose", "integer", "specify runtime debug verbosity level (0 - off, 1..6 for increased verbosity)"),
        OPTION ("novacache", "boolean", "specify runtime execution mode to use caching (default: false)"),
		OPTION_NULL
	}
};

#include <corenova/data/configuration/ini.h>
#include <corenova/data/configuration/xform.h>
#include <corenova/data/processor.h>
#include <corenova/data/array.h>
#include <corenova/sys/debug.h>
#include <corenova/sys/getopts.h>
#include <corenova/sys/signals.h>
#include <corenova/sys/transform.h>

#include <unistd.h>

/* command line options */
static char *id = NULL;
static char *engine = NULL;
static char *logdir = NULL;
static char *debug_level = NULL;
static char verbose = 

int main(int argc, char **argv) {

    /* process command line arguments */
	parameters_t *params = I (OptionParser)->parse(&this,argc,argv);
	if (params && params->count) {
        id           = I (Parameters)->getValue (params,"id");
		engine       = I (Parameters)->getValue (params,"engine");
		transform    = I (Parameters)->getValue (params,"transform");
        logdir       = I (Parameters)->getValue (params,"logdir");
        char verbose = I (Parameters)->getNumValue (params,"verbose");
        NovaCache    = I (Parameters)->getBooleanValue (params,"novacache");

        /* process the global settings */
        I (Debug)->setup (id,logdir,verbose);
        
        if (transform) {
            configuration_t *conf = I (XformConfigParser)->parse (config_file);
			if (conf) {
                char *confout = I (Configuration)->toString (conf);
                printf ("\nSTAGE 1: AFTER XFORM CONFIG PARSER (using %s)\n",config_file);
                printf ("------------------------------------------------------------------------------\n");
                printf ("%s\n",confout);
                
                /* main operation */
                transform_engine_t *teng = I (TransformEngine)->new (conf);
                if (teng) {
                    printf ("\nSTAGE 2: AFTER XFORM ENGINE PROCESSING (using %s)\n",config_file);
                    printf ("------------------------------------------------------------------------------\n");
                    I (TransformEngine)->printRules (teng);

                    printf ("\nSTAGE 3: AFTER XFORM SYMBOL EXPANSION PROCESSING (using %s)\n",config_file);
                    printf ("------------------------------------------------------------------------------\n");
                    transformation_matrix_t *matrix = I (TransformEngine)->resolve (teng);
                    I (TransformEngine)->destroy (&teng);
                    if (matrix) {
                        printf ("\nSTAGE 4: FINAL XFORM MATRIX EXECUTION PLAN (using %s)\n",config_file);
                        printf ("------------------------------------------------------------------------------\n");
                        I (TransformationMatrix)->print (matrix);
                        sleep (5);
                        I (TransformationMatrix)->destroy (&matrix);
                    }
                }
                
                return 0;
            } else {
                DEBUGP (DFATAL,"main","cannot retrieve configuration from %s",config_file);
            }
        }  else {
			DEBUGP (DFATAL,"main","must pass in --config_file argument!");
		}
    } else {
		DEBUGP (DFATAL,"main","run with -h for help in running this program.");
	}
    return 1;
}

