#include <corenova/source-stub.h>

THIS = {
	.version = "2.0",
	.author  = "Peter K. Lee <saint@corenova.com>",
	.description = "This module handles debug messages.",
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

        asprintf (&dOut,"%s/debug.out",dir);
        asprintf (&dErr,"%s/debug.err",dir);

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

IMPLEMENT_INTERFACE (Debug) = {
    .logDir = debugLogDir,
};
