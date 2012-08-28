#ifndef __streams_H__
#define __streams_H__

#include <corenova/interface.h>
#include <corenova/data/list.h>
#include <corenova/data/object.h>

#define STREAM_PAGE_SIZE 4096

#define INET INT32N

enum {

    INT8 = 1,
    INT16 = 2,
    INT32 = 3,
    INT64 = 4,
    FLOAT8 = 5,
    FLOAT16 = 6,
    FLOAT32 = 7,
    FLOAT64 = 8,
    STRING = 9,
    BINARY_STRING = 10,
    INT16N = 11,
    INT32N = 12,
    INT64N = 13

} types;

enum {

    L32 = 1,
    B32 = 2,
    L64 = 3,
    B64 = 4

} platforms;

typedef struct {

    uint32_t	local_platform;
    uint32_t	remote_platform;
    uint32_t	offset;
    uint32_t	size;
    
    list_t	*buffer;
    void 	*page;

    enum _type {
	T_READ,
	T_WRITE
    } type;
    
} stream_handle_t;

DEFINE_INTERFACE (StreamWriter) {

	int   (*write)		(stream_handle_t *, int);
	void  (*append)		(stream_handle_t *, void *, char, int);
	void  (*appendArray)	(stream_handle_t *, void **, char, int);
	void  (*appendArray2)	(stream_handle_t *, void **, char*, int);
	void  (*reset)		(stream_handle_t *);
	void  (*destroy)	(stream_handle_t **);
	void  *(*serialize)     (stream_handle_t *);
	binary_t *(*toBinary)   (stream_handle_t *, char *);
	stream_handle_t		*(*new) (void);
	
};

DEFINE_INTERFACE (StreamReader) {

	int  (*read)		(stream_handle_t *, int, int);
	int  (*fetch)		(stream_handle_t *, void **, char *, int);
	int  (*fetchArray)	(stream_handle_t *, void **, char *, int);
	void (*reset)		(stream_handle_t *);
	void (*destroy)		(stream_handle_t **);
	stream_handle_t		*(*new) (void *, int);
	stream_handle_t		*(*fromBinary) (binary_t*);

};

#endif
