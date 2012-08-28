#ifndef __file_H__
#define __file_H__

#include <corenova/interface.h>

#include <sys/stat.h>
#include <fcntl.h>  /* for open() and O_NONBLOCK */

typedef struct {
    
	char *name;      /* file name */

	boolean_t eof;
	
	FILE *fp;					/* file stream handle  */

	u_int32_t size;				/* on init file size */
	u_int32_t readBytes;		/* total read bytes  */
	u_int32_t writeBytes;		/* total write bytes */

    u_int32_t nread;            /* number of last read count */
    
	char  *line;			   /* buffer that holds getline results */
	size_t lineLen;				/* stores length of current line */
	
} file_t;

/*//////// MODULE DECLARATION //////////////////////////////////////////*/

/*
  put together the function interface available for export so that other
  modules can call them!
*/
DEFINE_INTERFACE (File)
{
	file_t     *(*new)     (const char *name, const char *mode);
	void        (*destroy) (file_t **);
	boolean_t   (*read)    (file_t *, void **buf, u_int32_t size, u_int16_t count);
	boolean_t   (*write)   (file_t *, void *buf,  u_int32_t size, u_int16_t count);
	boolean_t   (*getpos)  (file_t *, fpos_t *);
	boolean_t   (*setpos)  (file_t *, fpos_t *);
	boolean_t   (*stat)    (file_t *, struct stat *);
	void        (*rewind)  (file_t *);
	void        (*truncate)(file_t *);
	boolean_t   (*isEOF)   (file_t *);
	char       *(*getline) (file_t *, boolean_t multiline);
};

#endif
