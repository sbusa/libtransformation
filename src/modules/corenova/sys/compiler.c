#include <corenova/source-stub.h>

THIS = {
	.version = "2.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "This module enables dynamic c compilation and preprocessing.",
	.implements  = LIST ("DynamicCompiler"),
	.requires    = LIST ("corenova.data.string")
};

#include <corenova/sys/compiler.h>
#include <corenova/data/string.h>

static tinycc_t *
_newTinycc (const char *opts) {
    tinycc_t *tcc = NULL;
    if (tcc) {
        tcc->s = tcc_new ();
        if (tcc->s) {
            if (opts) {
                tcc_set_options (tcc->s,opts); /* handle error condition here? */
            }
        } else {
            DEBUGP (DERR,"_newTinycc","unable to allocate new TCC State object!");
            I (DynamicCompiler)->destroy (&tcc);
        }
    }
    return tcc;
}

static file_t *
_preprocess (tinycc_t *tcc, const char *filename) {
    if (tcc) {
        // set to some dynamic output file location
        tcc_set_options (tcc->s,"-o /tmp/preprocess");
        
        if (tcc_preprocess_file (tcc->s, filename) >= 0) {
            return I (File)->new ("/tmp/preprocess", "r"); 
        }
    }
    DEBUGP (DERR,"_preprocess","Unable to preprocess '%s' using tiny cc!",filename);
    return NULL;
}

static void
_destroy (tinycc_t **ptr) {
    if (ptr) {
        tinycc_t *tcc = *ptr;
        if (tcc) {
            tcc_delete (tcc->s);
            free (tcc);
            *ptr = NULL;
        }
    }
}

IMPLEMENT_INTERFACE (DynamicCompiler) = {
    .new           = _newTinycc,
    .preprocess    = _preprocess,
    .destroy       = _destroy
};
