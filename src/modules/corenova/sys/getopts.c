#include <corenova/source-stub.h>

THIS = {
	.version = "2.0",
	.author  = "Peter K. Lee <saint@corenova.com>",
	.description = "This module provides passed in argument processing.",
	.implements = LIST ("OptionParser"),
	.requires = LIST ("corenova.data.parameters")
};

#include <corenova/sys/getopts.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <stdarg.h>
#include <errno.h>
#include <ctype.h>				/* for isprint32_t */
#include <unistd.h> 			/* for fork */

static void
_printUsage(module_t *info) {
	int32_t idx = 0;
	printf("%s (Version: %s)\n",info->name,info->version);
	printf("Author (C) %s\n",info->author);
	printf("Description: %s\n",info->description);
	printf("-------------------------------------------------\n");
	printf("Master Parameters:\n");
	printf("\t-h --help\tThis help page\n");
	printf("\t-v --version\tPrint version information\n");
	printf("\t-D --daemon\tDaemonize (fork into background)\n");
	printf("\t-L --logdir\tSend all output to directory\n\n");
	printf("Program Parameters:\n");
	while (info->options[idx].name) {
		printf("\t--%s%c<%s>  [%s]\n",
			   info->options[idx].name,
			   (info->options[idx].value?'=':' '),
			   (info->options[idx].value?info->options[idx].value:"on|off"),
			   info->options[idx].desc);
		idx++;
	}
	printf("-------------------------------------------------\n");
}

#define DEVNULL "/dev/null"

static void
_daemonize(void) {
	pid_t pid; FILE *out,*err;
	if (getppid() == 1) return;
	pid = fork();
	if (pid < 0) exit(1);		/* fork error */
	if (pid > 0) exit(0);		/* parent exit */
	setsid(); 					/* obtain new process group */
    DEBUGP (DINFO,"daemonize","born again as PID:%u, closing stdin/stdout/stderr...",(unsigned int) getppid ());
	close (STDIN_FILENO);
	out = fopen(DEVNULL,"a");
	err = fopen(DEVNULL,"a");
	if (!out || !err ||
        dup2 (fileno (out), fileno (stdout)) < 0 ||
        dup2 (fileno (err), fileno (stderr)) < 0) {
		fprintf(stderr,"cannot daemonize output(s) to " DEVNULL ": %s",strerror(errno));
		exit(1);
	}
}

static boolean_t
_extractLongOpt(char *longOpt, char **key, char **val) {
	if (longOpt) {
		char *name = strstr(longOpt,"--");
		char *value = NULL;
		if (name) {
			name += 2;
			value = strchr(name,'=');
			if (value) {
				*value = '\0';
				*key = strdup(name);
				*val = strdup(value+1);
				*value = '='; 	/* put it back to what it was */
			} else {
				*key = strdup(name);
				*val = strdup("true"); /* what should I set this to be? */
			}
			return TRUE;
		}
	}
	return FALSE;
}

static boolean_t
_isValidOpt(module_t *info, char *optName) {
	if (info) {
		int32_t idx = 0;
		size_t optLen = strlen(optName);
		while (info->options[idx].name) {
			size_t newLen = strlen(info->options[idx].name);
			if ((optLen == newLen) &&
				(!strncasecmp(info->options[idx].name,optName,optLen)))
				return TRUE;
			idx++;
		}
	}
	return FALSE;
}

/*//////// INTERFACE ROUTINES //////////////////////////////////////////*/

static parameters_t *
_parseCommandLineArgs (module_t *info, int32_t argc, char **argv) {
	if (info) {
		parameters_t *params = I (Parameters)->new();
		if (params) {
			int32_t argch;
			int32_t daemon = 0;
			
			opterr = 0; /* turn off error messages */
			while ((argch = getopt_long(argc,argv, "DL:hv", StandardOpts, NULL)) != -1) {
				switch (argch) {
                  case 'L': I (Parameters)->update (params,"logdir",optarg); break;
                  case 'D': daemon = 1; break;
                  case 'v':
                      printf("%s (Version: %s)\n",info->name,info->version);
                      exit(0); break;
                  case 'h': _printUsage(info); exit(0); break;
                  case '?':
                  default:
                      if (isprint(optopt)){
                          if (argch == ':')
                              printf("`-%c' requires an argument!\n", optopt);
                          else
                              printf("invalid option `-%c'.\n", optopt);
                      }
                      else {
                          char *optName, *optValue;
                          if (_extractLongOpt(argv[optind-1],&optName,&optValue)) {
                              /* check if the given optName is part of
                               * accepted parameter */
                              if (_isValidOpt(info,optName)) {
                                  I (Parameters)->update(params,optName,optValue);
                                  free (optName); free(optValue);
                                  continue;
                              } else {
                                  printf("'%s' is not a valid argument!\n",optName);
                              }
                          }
                          printf("unable to parse long opt '%s'\n",argv[optind-1]);
                      }
                      printf("try '-h' for program usage details.\n");
                      exit(1);
				}
			}
			if (daemon) _daemonize();
			return params;
		} else {
			DEBUGP(0,"_parse","cannot create parameters_t data container!");
		}
	} else {
		DEBUGP(0,"_parse","parameter initialization data not passed in!");
	}
	return NULL;
}

/*
  initialize the module's function interface with actual functions!
*/
IMPLEMENT_INTERFACE (OptionParser) = {
	.parse   = _parseCommandLineArgs
};
