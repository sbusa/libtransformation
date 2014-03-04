#include <corenova/source-stub.h>

THIS = {
	.version = "2.0",
	.author  = "Peter K. Lee <saint@corenova.com>",
	.description = "This module processes xform configuration by running c preprocessing followed by ini processing",
	.implements = LIST ("XformConfigParser"),
	.requires = LIST ("corenova.sys.compiler",
                      "corenova.data.configuration.ini")
};

#include <corenova/sys/compiler.h>
#include <corenova/data/configuration/xform.h>
#include <corenova/data/configuration/ini.h>

/*//////// INTERNAL CODE //////////////////////////////////////////*/


/*//////// INTERFACE ROUTINES //////////////////////////////////////////*/

/* a convenience function of returning the configuration based on a filename */
static configuration_t *
_parse (const char *filename) {
    tinycc_t *tcc = I (DynamicCompiler)->new (NULL);
    if (tcc) {
        file_t *file = I (DynamicCompiler)->preprocess (tcc,filename);
        configuration_t *conf = I (IniConfigParser)->parseByFile (file);
        I (File)->destroy (&file);
        return conf;
    } else {
        DEBUGP (DERR,"_parse","unable to create an instance of tinycc_t");
    }
    return NULL;
}

static boolean_t
_writeFile (const char *filename, configuration_t *conf) {

    return I (IniConfigParser)->write (filename,conf);
}

IMPLEMENT_INTERFACE (XformConfigParser) = {
    .parse         = _parse,
	.write         = _writeFile
};
