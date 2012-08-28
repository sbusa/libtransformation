#ifndef __md5_H__
#define __md5_H__

#include <corenova/interface.h>

typedef struct {

    uint32_t state[4];            /* state (ABCD) */
    uint32_t count[2];     /* number of bits, modulo 2^64 (lsb first) */
    unsigned char buffer[64];     /* input buffer */
    
} md5_ctx_t;

typedef unsigned char md5_t;

DEFINE_INTERFACE (MD5) {
    
    md5_t *(*new)      (void);
    void   (*destroy)  (md5_t **);
    int    (*compare)  (const md5_t *, const md5_t *);
    char  *(*toString) (md5_t *); /* returns a new string, need to free afterwards */
    
};

DEFINE_INTERFACE (MD5Transform) {
    
    md5_ctx_t *(*new)     (void);
    void       (*update)  (md5_ctx_t *, unsigned char *, unsigned int);
    md5_t     *(*final)   (md5_ctx_t *);
    void       (*destroy) (md5_ctx_t **);
    
};

#endif
