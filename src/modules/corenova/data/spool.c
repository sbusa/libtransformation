#include <corenova/source-stub.h>

THIS = {
	.version = "3.0",
	.author  = "Peter K. Lee <saint@corenova.com>",
	.description = "This module enables operations on spool (log) files.",
	.implements = LIST ("Spool","Transformation"),
	.requires   = LIST ("corenova.data.file",
                        "corenova.data.glob",
                        "corenova.sys.transform"),
    .transforms = LIST ("transform:feeder -> data:file:spool",
                        "data:file:spool -> data:file:spool::*")
};

#include <corenova/data/spool.h>
#include <corenova/data/glob.h>
#include <corenova/sys/transform.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

static void
_writeWaldoPosition (spool_t *spool) {
	I_TYPE(File) *I_FILE = I (File);
	if (spool->waldo) { /* using a waldo file */
		fpos_t position;
		I_FILE->getpos(spool->file,&position);
		I_FILE->rewind(spool->waldo);
		/* check for error? */
		I_FILE->write(spool->waldo,&position,sizeof(fpos_t),1);
	}
}

static boolean_t
_handleSpoolError(spool_t *spool) {
	I_TYPE(File) *I_FILE = I (File);
	if (spool) {
        /*
           In an EOF condition, we want to know if there is a newer
           file to make a jump to.

           The scenario is that the current file being parsed may have
           been moved, or even removed from the system.

           Only thing that we can use to check any new files globbed
           vs. current file is the current file's inode number. (since
           if the current file is removed, we won't be able to glob
           for it anymore)
        */
		if (I_FILE->isEOF(spool->file)) {
            struct stat st;
            if (I (File)->stat (spool->file, &st)) {
                char *currentFileName = I (Glob)->nameByInode (spool->name,st.st_ino);
                char *nextFileName = NULL;
                
                if (currentFileName) {
                    nextFileName = I (Glob)->nextNewerFile (spool->name,&st);
                } else {
                    /* Our file is gone... whatever next Glob finds next is our baby! */
                    nextFileName = I (Glob)->nextNewerFile (spool->name,NULL);
                }
                
                if (nextFileName) {
                    I (File)->destroy (&spool->file);
                    spool->file = I (File)->new(nextFileName,"r+");
                    if (spool->file) {
                        DEBUGP (DINFO,"_handleSpoolError","migrating up to newer file, %s",nextFileName);
                        free (nextFileName);
                    
                        if (currentFileName) {
                            DEBUGP (DINFO,"_handleSpoolError","getting rid of older file, %s",currentFileName);
                            remove(currentFileName);
                            free(currentFileName);
                        }
                        return TRUE;
                    } else {
                        DEBUGP(1,"_handleSpoolError","ERROR: cannot access next spool file '%s'",
                               nextFileName);
                    }
                } else {
                    /* Nothing to migrate up to... */
                    if (spool->truncate && st.st_size) {
                        /* eat up the contents since, well, we already went through it all! */
                        I (File)->truncate (spool->file);
                    }
                }
                /* clean up these names since we don't need them anymore! */
				if (currentFileName)
					free(currentFileName);
				if (nextFileName)
					free(nextFileName);
            }
		}
	}
	return FALSE;
}

/*//////// INTERFACE ROUTINES //////////////////////////////////////////*/

static spool_t *
_new (const char *name, boolean_t enableTruncate) {
	spool_t *s = (spool_t *)calloc(1,sizeof(spool_t));
	
	if (s) {
        char *nextFileName = I (Glob)->nextNewerFile (name,NULL);
	
        if (nextFileName) {
            if ((s->file = I (File)->new (nextFileName,"r+"))) {
                DEBUGP (DINFO,"new","successfully opened '%s' (for %s) spool file!",nextFileName,name);
                s->name = strdup (name);
                s->truncate = enableTruncate;
            } else {
	    
		DEBUGP (DERR,"new","failed to open '%s' (for %s) spool file!",nextFileName,name);
                I (Spool)->destroy (&s);
            }
            free (nextFileName);
        } else {
            I (Spool)->destroy (&s);
        }
	}
	return s;
}

static void
_destroy (spool_t **sPtr) {
    if (sPtr) {
        spool_t *spool = *sPtr;
        if (spool) {
            I (File)->destroy (&spool->file);
            I (File)->destroy (&spool->waldo);
            free(spool->name);
            free(spool);
            *sPtr = NULL;
        }
	}
}

static boolean_t
_read (spool_t *spool, void **buf, u_int32_t size, u_int16_t count) {
	if (spool) {
	  try_again:
		if (I (File)->read(spool->file,buf,size,count)) {
			_writeWaldoPosition(spool);
			return TRUE;
		}
		if (_handleSpoolError(spool)) goto try_again;
	}
	return FALSE;
}

static char *
_getline (spool_t *spool, boolean_t multiline) {
	if (spool) {
		char *line = NULL;
	  try_again:
		if ((line = I (File)->getline(spool->file, multiline))) {
			_writeWaldoPosition(spool);
			return line;
		}
		if (_handleSpoolError(spool)) goto try_again;
	}
	return NULL;
}

static boolean_t
_useWaldoFile (spool_t *spool, const char *name) {
	if (spool && name) {
		spool->waldo = I (File)->new (name,"r+");
		if (spool->waldo) {
			fpos_t *position = NULL;
			if (I (File)->read(spool->waldo,(void *)&position,sizeof(fpos_t),1) == 1) {
				DEBUGP(DINFO,"_useWaldoFile","jumping to saved waldo location from '%s'",name);
				I (File)->setpos(spool->file,position);
			}
			free(position);
			
			return TRUE;
		} else {
            spool->waldo = I (File)->new (name,"w+");
            if (spool->waldo) {
                DEBUGP (DINFO,"_useWaldoFile","created a new waldo file at '%s'",name);
                return TRUE;
            } else {
                DEBUGP(DERR,"_useWaldoFile","cannot access '%s' waldo file.",name);
            }
		}
	}
	return FALSE;
}

/*
  initialize the module's function interface with actual functions!
*/
IMPLEMENT_INTERFACE (Spool) = {
	.new          = _new,
	.destroy      = _destroy,
	.read         = _read,
	.getline      = _getline,
	.useWaldoFile = _useWaldoFile
};

/*//////// Transformation Interface Implementation //////////////////////////////////////////*/

TRANSFORM_EXEC (feeder2spool) {
    spool_t *spool = (spool_t *)xform->instance;
    if (spool) {
        transform_object_t *obj = I (TransformObject)->new ("data:file:spool",spool);
        if (obj) I (TransformObject)->save (obj); /* save since copy of xform->instance */
        return obj;
    }
    return NULL;
}

TRANSFORM_EXEC (spool2spoolformat) {
    spool_t *spool = (spool_t *)in->data;
    if (spool) {
        transform_object_t *obj = I (TransformObject)->new (xform->to,spool);
        if (obj) I (TransformObject)->save (obj); /* save since copy of in->data */
        return obj;
    }
    return NULL;
}
TRANSFORM_NEW (newSpoolTransformation) {
    
    TRANSFORM ("transform:feeder", "data:file:spool", feeder2spool);
    TRANSFORM ("data:file:spool", "data:file:spool::*", spool2spoolformat);

    IF_TRANSFORM (feeder2spool) {
        boolean_t autotruncate = FALSE;
        if (I (Parameters)->getValue (blueprint,"auto_truncate") &&
            I (String)->equal ("yes",I (Parameters)->getValue (blueprint,"auto_truncate"))) {
            autotruncate = TRUE;
        }
        spool_t *spool = I (Spool)->new (I (Parameters)->getValue (blueprint,"file_name"),autotruncate);
        if (spool) {
            if (I (Parameters)->getValue (blueprint,"waldo_file")) {
                if (!I (Spool)->useWaldoFile (spool,I (Parameters)->getValue (blueprint,"waldo_file"))) {
                    I (Spool)->destroy (&spool);
                }
            }
        }
        TRANSFORM_WITH (spool);
    }
    
} TRANSFORM_NEW_FINALIZE;

TRANSFORM_DESTROY (destroySpoolTransformation) {

    IF_TRANSFORM (feeder2spool) {
        spool_t *spool = (spool_t *) xform->instance;
        I (Spool)->destroy (&spool);
    }
    
} TRANSFORM_DESTROY_FINALIZE;

IMPLEMENT_INTERFACE (Transformation) = {
	.new       = newSpoolTransformation,
	.destroy   = destroySpoolTransformation,
	.execute   = NULL,
	.free      = NULL
};
