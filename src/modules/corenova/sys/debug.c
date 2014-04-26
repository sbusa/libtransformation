#include <corenova/source-stub.h>

THIS = {
	.version = "2.0",
	.author  = "Peter K. Lee <saint@corenova.com>",
	.description = "This module handles debug messages.",
    .requires = LIST ("corenova.data.string"),
	.implements = LIST ("Debug")
};

#include <corenova/sys/debug.h>
#include <corenova/data/string.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <unistd.h>             /* for dup2 */
#include <sys/stat.h>
#include <errno.h>

static void debugLogDir (const char *dir) {
    if (dir) {
        char *dOut, *dErr;

        asprintf (&dOut,"%s/default.out",dir);
        asprintf (&dErr,"%s/default.err",dir);

        FILE *out = fopen (dOut,"a");
        FILE *err = fopen (dErr,"a");

        free (dOut);
        free (dErr);
        
        if (out && err &&
            dup2 (fileno (out), fileno (stdout)) >= 0 &&
            dup2 (fileno (err), fileno (stderr)) >= 0) {
            setvbuf (out,NULL,_IOLBF,BUFSIZ);
            setvbuf (err,NULL,_IOLBF,BUFSIZ);
        } else {
            DEBUGP (DERR,"logDir","unable to redirect STDOUT & STDERR to %s directory",dir);
        }
    }
}

static void redirectfd (int fd, const char *to) {
    if (to) {
        FILE *redirect = fopoen (to,"a");
        if (redirect && dup2 (fileno (redirect), fd) >= 0)
            setvbuf (redirect,NULL,_IOLBF,BUFSIZ);
    }
}

static void setupDebug (const char *id, const char *logdir, const int level) {
    if (level > 0) {
        char *debug_levels[] = DEBUG_LEVELS;
        DebugLevel = level;
        DEBUGP(DINFO, "debug", "setting verbosity level to %s", debug_levels[DebugLevel]+1);
    }

    if (logdir) {
        char *out = I (String)->new ("%s/%s.out",logdir,id?id:"debug");
        if (out) {
            redirectfd (stdout,out);
        }
        DEBUGP(DINFO, "debug", "setting log output to %s for %s instance", logdir, id?id:"default");
    }
    
}

IMPLEMENT_INTERFACE (Debug) = {
    .logDir = debugLogDir,
    .setup  = setupDebug
};
