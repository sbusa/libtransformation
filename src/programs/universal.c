#include <corenova/source-stub.h>

THIS = {
	.name = "Universal Binary",
	.version = "2.0",
	.author = "Peter K. Lee <saint@corenova.com>",
	.description = "This program will be the only binary you will ever need.",
	.requires = LIST ("corenova.data.configuration.ini",
                      "corenova.data.processor",
                      "corenova.sys.debug",
                      "corenova.sys.getopts",
                      "corenova.sys.signals"),
	.options = {
		OPTION ("config_file", "filename", "name of configuration ini file"),
		OPTION ("logdir", "directory", "log directory to write debug outputs"),
        OPTION ("debug_level", "integer", "specify runtime debug level (0 - off, 1..6 for increased verbosity)"),
        OPTION ("novacache", "boolean", "specify runtime execution mode to use caching"),
		OPTION_NULL
	}
};

#include <corenova/sys/quark.h>
#include <corenova/data/configuration/ini.h>
#include <corenova/data/processor.h>
#include <corenova/sys/debug.h>
#include <corenova/sys/getopts.h>
#include <corenova/sys/signals.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <unistd.h> 			/* for sleep and kill */

/* command line options */
static char *config_file = NULL;
static char *logdir = NULL;
static char *debug_level = NULL;

/* global structures */
static data_processor_t *processor = NULL;

static void
_signalDebugLevelChange(void) {
    char *debug_levels[] = DEBUG_LEVELS;

    if(++DebugLevel > DALL)
        DebugLevel = DOFF;

    fprintf(stdout, "changing debug level to %s....", debug_levels[DebugLevel]);
    fprintf(stderr, "changing debug level to %s....", debug_levels[DebugLevel]);
}

static void
_signalIgnore (void) {
	printf("ignoring signal...\n");
}

static void
_signalConfigReload (void) {
    if (config_file && processor) {
        configuration_t *conf = I (IniConfigParser)->parse (config_file);
        I (DataProcessor)->reload (processor,conf);
    }
}

static void
_signalCleanShutdown (void) {
	SystemExit = TRUE;
    DEBUGP (DWARN,"signal","executing shutdown...");
//    I (Quark)->killAll ();
//    I (Signal)->handler (SIGINT, _signalIgnore);
}

static void
executeProgram (configuration_t *conf) {
	if (conf) {
        processor = I (DataProcessor)->new (conf);
        if (processor) {
            if (I (DataProcessor)->start (processor)) {
                DEBUGP (DDEBUG,"executeProgram","Data Processor running...");

                while (!SystemExit) {
                    // noop
                    sleep (1);
                }
                
                DEBUGP (DDEBUG,"executeProgram","Data Processor exiting...");
                I (DataProcessor)->stop (processor);
            }
            DEBUGP (DDEBUG,"executeProgram","Data Processor is being destroyed...");
            I (DataProcessor)->destroy (&processor);
        }
		I (Configuration)->destroy (&conf);
	}
}

/*
 * General Purpose code main entrance point
 */
int main(int argc, char **argv)
{
	/* register global signals */
	//I (Signal)->handler(SIGALRM, _signalSessionCleanup);
	I (Signal)->handler(SIGPIPE, _signalIgnore);
	I (Signal)->handler(SIGWINCH,_signalIgnore);
    I (Signal)->handler(SIGHUP,  _signalConfigReload);
	I (Signal)->handler(SIGINT,  _signalCleanShutdown);
	I (Signal)->handler(SIGTERM, _signalCleanShutdown);
    I (Signal)->handler(SIGUSR1, _signalDebugLevelChange);

	/* process command line arguments */
	parameters_t *params = I (OptionParser)->parse(&this,argc,argv);
	if (params && params->count) {
		config_file = I (Parameters)->getValue (params,"config_file");
        debug_level = I (Parameters)->getValue (params,"debug_level");
        logdir      = I (Parameters)->getValue (params,"logdir");
        NovaCache   = I (Parameters)->getBooleanValue (params,"novacache");

        if (NovaCache) {
            printf ("** turned on NovaCache **\n");
        }

        
		if (config_file) {
            configuration_t *conf = I (IniConfigParser)->parse (config_file);
			if (conf) {
				category_t *global = I (Configuration)->getCategory (conf,"Global");
				if (global) {
                    if (debug_level) {
						/* override configuration file setting */
						I (Category)->setParameter (global,"debug_level",debug_level);
                    }
                    if (logdir) {
						/* override configuration file setting */
						I (Category)->setParameter (global,"logdir",logdir);
                    }
                    /* use the configuration file settings */
                    debug_level  = I (Category)->getParamValue(global, "debug_level");
                    logdir       = I (Category)->getParamValue(global, "logdir");
				}
                
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

				executeProgram (conf);
				DEBUGP (DINFO,"main","finished execution!");
                
                // XXX - this is to ensure that we truly die.
                kill (getpid (),SIGKILL);
				return 0;
                
			} else {
				DEBUGP (DFATAL,"main","cannot retrieve configuration from %s",config_file);
			}
		} else {
			DEBUGP (DFATAL,"main","must pass in --config_file argument!");
		}
	} else {
		DEBUGP (DFATAL,"main","run with -h for help in running this program.");
	}
	return 1;
}
