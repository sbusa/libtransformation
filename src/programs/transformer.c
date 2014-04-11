#include <corenova/source-stub.h>

THIS = {
	.name = "A Transformation Engine Tester",
	.version = "1.0",
	.author = "Peter K. Lee <saint@corenova.com>",
	.description = "This program should allow you to see the transformation engine in action.",
	.requires = LIST ("corenova.data.configuration.xform",
                      "corenova.data.array",
                      "corenova.sys.debug",
                      "corenova.sys.getopts",
                      "corenova.sys.transform"),
	.options = {
		OPTION ("config_file", "filename", "name of configuration ini file"),
		OPTION ("logdir", "directory", "log directory to write debug outputs"),
        OPTION ("debug_level", "integer", "specify runtime debug level (0 - off, 1..6 for increased verbosity)"),
		OPTION_NULL
	}
};

#include <corenova/data/configuration/xform.h>
#include <corenova/data/array.h>
#include <corenova/sys/debug.h>
#include <corenova/sys/getopts.h>
#include <corenova/sys/transform.h>

#include <unistd.h>

/* command line options */
static char *config_file = NULL;
static char *logdir = NULL;
static char *debug_level = NULL;

int main(int argc, char **argv) {

    /* process command line arguments */
	parameters_t *params = I (OptionParser)->parse(&this,argc,argv);
	if (params && params->count) {
		config_file = I (Parameters)->getValue (params,"config_file");
        debug_level = I (Parameters)->getValue (params,"debug_level");
        logdir      = I (Parameters)->getValue (params,"logdir");

        /* process the global settings */
        if (debug_level) {
            char *debug_levels[] = DEBUG_LEVELS;
            DebugLevel = atoi(debug_level);
            DEBUGP(DINFO, "main", "setting debug level to %s", debug_levels[DebugLevel]+1);
        }
        if (logdir) {
            I (Debug)->logDir (logdir);
            DEBUGP(DINFO, "main", "setting log output to %s", logdir);
        }
        if (config_file) {
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

