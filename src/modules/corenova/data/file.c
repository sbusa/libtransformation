#include <corenova/source-stub.h>

THIS = {
	.version = "2.0",
	.author  = "Peter K. Lee <saint@corenova.com>",
	.description = "This module enables common filesystem operations.",
	.implements = LIST ("File"),
	.requires = LIST ("corenova.data.string")
};

#include <corenova/data/file.h>
#include <corenova/data/string.h>

#if !HAVE_GETLINE
// getline() replacement for Darwin and Solaris etc.
// This code uses backward seeks (unless rchunk is set to 1) which can't work
// on pipes etc. However, add2fs_from_file() only calls getline() for
// regular files, so a larger rchunk and backward seeks are okay.

ssize_t
getdelim(char **lineptr, size_t *n, int delim, FILE *stream)
{
        char *p;                    // reads stored here
        size_t const rchunk = 512;  // number of bytes to read
        size_t const mchunk = 512;  // number of extra bytes to malloc
        size_t m = rchunk + 1;      // initial buffer size

        if (*lineptr) {
                if (*n < m) {
                        *lineptr = (char*)realloc(*lineptr, m);
                        if (!*lineptr) return -1;
                        *n = m;
                }
        } else {
                *lineptr = (char*)malloc(m);
                if (!*lineptr) return -1;
                *n = m;
        }

        m = 0; // record length including seperator

        do {
                size_t i;     // number of bytes read etc
                size_t j = 0; // number of bytes searched

                p = *lineptr + m;

                i = fread(p, 1, rchunk, stream);
                if (i < rchunk && ferror(stream))
                        return -1;
                while (j < i) {
                        ++j;
                        if (*p++ == (char)delim) {
                                *p = '\0';
                                if (j != i) {
                                        if (fseek(stream, j - i, SEEK_CUR))
                                                return -1;
                                        if (feof(stream))
                                                clearerr(stream);
                                }
                                m += j;
                                return m;
                        }
                }

                m += j;
                if (feof(stream)) {
                        if (m) return m;
                        if (!i) return -1;
                }

                // allocate space for next read plus possible null terminator
                i = ((m + (rchunk + 1 > mchunk ? rchunk + 1 : mchunk) +
                      mchunk - 1) / mchunk) * mchunk;
                if (i != *n) {
                        *lineptr = (char*)realloc(*lineptr, i);
                        if (!*lineptr) return -1;
                        *n = i;
                }
        } while (1);
}
#define getline(a,b,c) getdelim(a,b,'\n',c)
#endif /* HAVE_GETLINE */

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <errno.h>
#include <unistd.h> /* for usleep() */

static boolean_t
_handleFileError(file_t *file, fpos_t *position) {
	if (feof(file->fp)){

		file->eof = TRUE;
//		file->retry++;

		clearerr(file->fp);

		usleep(1000); /* give it a breather... */

	}
	else if (ferror(file->fp)){

		switch(errno){ /* non-blocking does not issue EOF! */
		case EAGAIN:
			usleep(1000); /* give it a breather... */
			return TRUE;
		default:
//			file->shutdown = 1;
			DEBUGP(DERR,"handleFileError","%s (%d)",strerror(errno),errno);
		}
	}
	else {
		DEBUGP(DERR,"handleFileError","Unknown FILE Error!!!");
	}
	/* reset position back to where it was before the error */
	fsetpos(file->fp,position);
	
	
	return FALSE;
}	

/*//////// INTERFACE ROUTINES //////////////////////////////////////////*/

static void
_destroy (file_t **fPtr) {
    if (fPtr) {
        file_t *file = *fPtr;
        if (file) {
            if (file->fp) fclose(file->fp);
            free(file->name);
            free(file->line);
            free(file);
            *fPtr = NULL;
        }
	}
}

static file_t *
_new (const char *name, const char *mode) {
	file_t *file = (file_t *)calloc(1,sizeof(file_t));
	if (file) {
		if (name) {
			file->fp = fopen(name,mode);
			if (file->fp) {
				struct stat st;
				file->name = strdup(name);
				stat(file->name,&st);
				file->size  = st.st_size;
				DEBUGP(DINFO,"new","%s opened successfully.",file->name);
			} else {
//			DEBUGP(DERR,"new","cannot open '%s' for '%s' operation", name, mode);
				I (File)->destroy(&file);
			}
		} else {
			DEBUGP (DERR,"new","cannot open file without passing in a name!");
			I (File)->destroy (&file);
		}
	}
	return (file);
}

static boolean_t
_read (file_t *file, void **buf, u_int32_t size, u_int16_t count) {
	if (file) {
		fpos_t position;
		if (!*buf &&
			((*buf = calloc(count,size)) == NULL )) return 0;
	
		fgetpos(file->fp,&position);
	  try_again:
		if ((file->nread = fread(*buf,size,count,file->fp)) == count){
			file->readBytes += (count*size);
			file->eof   = FALSE;
			return TRUE;
		}
		if (_handleFileError(file,&position)) goto try_again;
        
	}
	return FALSE;
}

static boolean_t
_write (file_t *file, void *buf, u_int32_t size, u_int16_t count) {
	if (file) {
		fpos_t position;
		fgetpos(file->fp,&position);
	  try_again:
		if (fwrite(buf,size,count,file->fp) == count){
			fflush(file->fp);
			file->writeBytes += (count*size);
			return TRUE;
		}
		if (_handleFileError(file,&position)) goto try_again;
	}
	return FALSE;
}

static boolean_t
_getpos (file_t *file, fpos_t *pos) {
	if (file && file->fp &&
		!fgetpos(file->fp,pos)) return TRUE;
	return FALSE;
}

static boolean_t
_setpos (file_t *file, fpos_t *pos) {
	if (file && file->fp &&
		!fsetpos(file->fp,pos)) return TRUE;
	return FALSE;
}

static boolean_t
_stat (file_t *file, struct stat *buf) {
	if (file && file->fp &&
		!fstat(fileno(file->fp),buf)) return TRUE;
	return FALSE;
}

static void
_rewind (file_t *file) {
	if (file && file->fp)
		rewind(file->fp);
}

static void
_truncate (file_t *file) {
	if (file && file->fp) {
		if (ftruncate(fileno(file->fp),0)!=-1)
			_rewind(file);
	}
}

static boolean_t
_isEOF (file_t *file) {
	return (file?file->eof:FALSE);
}

static char *
_getline (file_t *file, boolean_t multiline) {
	char *lineBuffer = NULL;
	fpos_t position;
	size_t len = 0;
	fgetpos(file->fp,&position);
  try_again:
	if (multiline){
		if (file->line) (*file->line) = '\0';
#if defined (freebsd6) || defined (freebsd7)
				    
		while((lineBuffer = fgetln(file->fp, &len))!=NULL) { // getline() is not a BSD, nor a POSIX function

		    char *tmpBuf = malloc(len+1);
		    memcpy(tmpBuf, lineBuffer, len);
		    tmpBuf[len] = '\0';
		    
		    lineBuffer = tmpBuf;

#else
		while(getline(&lineBuffer,&len,file->fp)!=-1){
#endif	

			char *ptr = strstr(lineBuffer,"\\\n");
			if (ptr) *ptr = '\0';
			I (String)->join(&file->line,lineBuffer);
			
			if (!ptr){

				free(lineBuffer);
				return file->line;

			}
			
		}
		
		free(lineBuffer);
		
	}
	else {
#if defined (freebsd6) || defined (freebsd7)
		if ((lineBuffer = fgetln(file->fp,&file->lineLen))!=NULL) {
		
			if(file->line) {
			    free(file->line);
			    file->line = NULL;
			}
		
			file->line = malloc(file->lineLen+1);		// fgetln() tends to re-use its buffer, so data must be duplicated
			memcpy(file->line, lineBuffer, file->lineLen);
			file->line[file->lineLen] = '\0';

			return file->line;			
			
		} else {
		
		    file->line = NULL;
		    file->lineLen = 0;
		
		}
#else
		if (getline(&file->line,&file->lineLen,file->fp)!=-1)
			return file->line;
#endif

	}
	/* failure case */
	if (_handleFileError(file,&position)) goto try_again;
	return NULL;
}

/*
  initialize the module's function interface with actual functions!
*/
IMPLEMENT_INTERFACE (File) = {
	.new      = _new,
	.destroy  = _destroy,
	.read     = _read,
	.write    = _write,
	.getpos   = _getpos,
	.setpos   = _setpos,
	.stat     = _stat,
	.rewind   = _rewind,
	.truncate = _truncate,
	.isEOF    = _isEOF,
	.getline  = _getline,
};
