#include <corenova/source-stub.h>

THIS = {
	.version = "2.0",
	.author  = "Peter K. Lee <saint@corenova.com>",
	.description = "This module represents a category of information.",
	.implements = LIST ("IniConfigParser"),
	.requires = LIST ("corenova.data.file","corenova.data.configuration")
};

#include <corenova/data/configuration/ini.h>

/*//////// INTERNAL CODE //////////////////////////////////////////*/


/*//////// INTERFACE ROUTINES //////////////////////////////////////////*/

static configuration_t *
_parseFile (file_t *file) {
    /* internal lookup caching of the common interfaces being used */
	I_TYPE(File)          *I_FILE     = I(File);
	I_TYPE(Configuration) *I_CONFIG   = I(Configuration);
	I_TYPE(Category)      *I_CATEGORY = I(Category);
	I_TYPE(String)        *I_STRING   = I(String);

    if (file) {
        configuration_t *conf = I_CONFIG->new(file->name);
        if (conf) {
			char *line = NULL, *ptr = NULL;
			int32_t lineNumber = 0;
			category_t *category = I_CONFIG->addCategory(conf,"global");
			while ((line = I_FILE->getline(file, TRUE))) {

				char *string = I_STRING->trim(line);
				lineNumber++;
				
				/* treat ';' as comment */
				ptr = strchr(string,';');
				if (ptr) *ptr = '\0';
				/* treat '#' as comment */
				ptr = strchr(string,'#');
				if (ptr) *ptr = '\0';
				if (!strlen(string)) {
				    continue; /* ignore empty lines */
				}

				if (*string == '[') {
					char *end = strchr(string,']');
					if (!end) {
						DEBUGP(DERR,"_parseFile","missing ']' detected on line #%d.",lineNumber);
						return FALSE;
					}
					*end = '\0';
					category = I_CONFIG->addCategory(conf,I_STRING->trim(string+1));
					if (*(end+1) != '\0') {
						DEBUGP(DINFO,"_parseFile","ignoring trailing garbage %s",end+1);
					}
				} else {
					char *sep = strchr(string,'=');
					if (!sep) {
						DEBUGP(DERR,"_parseFile","invalid line '%s' detected on line #%d.",line,lineNumber);
						return FALSE;
					}
					*sep = '\0';
					I_CATEGORY->setParameter(category,I_STRING->trim(string),I_STRING->trim(sep+1));
                }
            }
            return conf;
        } else {
            DEBUGP (DERR,"_praseFile","unable to create an instance of configuration object");
        }
    }
	return NULL;
}		

/* a convenience function of returning the configuration based on a filename */
static configuration_t *
_parse (const char *filename) {
    file_t *file = I (File)->new (filename,"ro");
    configuration_t *conf = _parseFile (file);
    I (File)->destroy (&file);
    return conf;
}

static boolean_t
_writeFile (const char *filename, configuration_t *conf) {
	I_TYPE(File)          *I_FILE     = I(File);
	I_TYPE(Configuration) *I_CONFIG   = I(Configuration);
	
	boolean_t ret = FALSE;
	if (conf) {
		file_t *file = I_FILE->new(filename,"w");
		if (file) {
			char *outText = I_CONFIG->toString(conf);
			if (outText) {
				ret = I_FILE->write(file,outText,strlen(outText),1);
				free(outText);
			}
			I_FILE->destroy(&file);
		} else {
			DEBUGP(0,"_writeFile","ERROR: cannot write to file %s",filename);
		}
	}
	return ret;
}

IMPLEMENT_INTERFACE (IniConfigParser) = {
    .parse         = _parse,
    .parseByFile   = _parseFile,
	.write         = _writeFile
};
