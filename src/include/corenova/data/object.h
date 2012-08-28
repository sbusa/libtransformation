#ifndef __object_H__
#define __object_H__

#include <corenova/interface.h>

typedef void object_interface_t;

DEFINE_INTERFACE (BinaryHandler) {
    void *(*serialize)   (void *);
    void *(*deserialize) (void *);
    void  (*free)        (void *);
};

typedef struct {

	uint32_t size;
	char ALIGNED64 *format;
	void ALIGNED64 *data;

} binary_t;

typedef struct {

	uint32_t size;
	char ALIGNED64 *format;
	char ALIGNED64 *data;
	
} text_t;

DEFINE_INTERFACE (BinaryObject) {
	binary_t *(*new)     (void *data, uint32_t size, const char *format);
    binary_t *(*clone)   (binary_t *);
	void      (*destroy) (binary_t **);
};

DEFINE_INTERFACE (TextObject) {
	text_t *(*new)     (char *data, uint32_t size, const char *format);
    text_t *(*clone)   (text_t *);
	void    (*destroy) (text_t **);
};

#endif	/* __object_H__ */
