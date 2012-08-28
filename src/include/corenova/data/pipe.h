#ifndef __data_pipe_H__
#define __data_pipe_H__

#include <corenova/interface.h>
#include <corenova/net/ssl.h>

/*----------------------------------------------------------------*/

#include <poll.h>

typedef enum {

    PIPE_POLLIN = 0,
    PIPE_POLLOUT = 1,
    PIPE_POLLINOUT = 2
    
} pipe_poll_type_t;

/* uni-directional stream */
typedef struct {

    struct pollfd fds[2]; /* 0 - in, 1 - out */
    uint32_t maxsize;
    
    /* private buffer */
    char *buf;

    char *bufend;
    char *dataend;
    char *pos;
    char *marker;               /* for cleaning optimization */
    ssl_t *ssl[2]; 
    
} data_pipe_t;

/*
 * PIPE DEFAULT VARIABLES
 */
#define PIPE_POLL_TIMEOUT 1                    /* 1ms timeout */
#define PIPE_BLOCK_SIZE   8192 * 2                 /* 8KB */
#define PIPE_BUF_MAXSIZE  PIPE_BLOCK_SIZE * 32 /* upto 256KB */

#define PIPE_CONTINUE    1
#define PIPE_COMPLETE    0
#define PIPE_BROKEN     -1
#define PIPE_NEEDWRITE  -2
#define PIPE_TIMEOUT    -3
#define PIPE_FATAL      -4

DEFINE_INTERFACE (DataPipe) {

    data_pipe_t  *(*new)           (int in, int out);
    boolean_t     (*fill)          (data_pipe_t *, char *buf, uint32_t size); /* pre-fill pipe */
    boolean_t     (*hasData)       (data_pipe_t *);
    int           (*poll)          (data_pipe_t *, pipe_poll_type_t type, int timeout);
    int           (*read)          (data_pipe_t *);
    int           (*write)         (data_pipe_t *);
    int           (*writeRegion)   (data_pipe_t *, char *spos, char *epos);
    int           (*flush)         (data_pipe_t *);
    /* the replace calls replaces the old in/out and returns them */
    int           (*replaceInput)  (data_pipe_t *, int newIn);
    int           (*replaceOutput) (data_pipe_t *, int newOut);
    void          (*setMaxSize)    (data_pipe_t *, uint32_t size);
    void          (*setMarker)     (data_pipe_t *, char *pos);
    void          (*clean)         (data_pipe_t *);
    void          (*cleanBefore)   (data_pipe_t *, char *upto);
    void          (*reset)         (data_pipe_t *);
    void          (*reverse)       (data_pipe_t *);
    void          (*destroy)       (data_pipe_t **);
    
};

/*----------------------------------------------------------------*/

#include <corenova/net/socket.h>

typedef struct {

    socket_pair_t *pair;
    
    int in2out[2];
    int out2in[2];
    
} stream_intercept_t;

typedef struct {

    data_pipe_t *in2out;
    data_pipe_t *out2in;

    enum initiator {
        PIPESTREAM_UNKNOWN = 0,
        PIPESTREAM_IN2OUT,
        PIPESTREAM_OUT2IN
    } initiator;

    int timeout;                /* poll timeout in msecs */

#define PIPESTREAM_POLL_TIMEOUT -1 /* infinite timeout */

} data_pipe_stream_t;

DEFINE_INTERFACE (DataPipeStream) {

    data_pipe_stream_t *(*new)        (data_pipe_t *in2out, data_pipe_t *out2in);
    enum initiator      (*poll)       (data_pipe_stream_t *, int timeout);
    boolean_t           (*sync)       (data_pipe_stream_t *);
    void                (*flush)      (data_pipe_stream_t *);
    void                (*initiator)  (data_pipe_stream_t *, enum initiator);
    void                (*setTimeout) (data_pipe_stream_t *, int timeout);
    void                (*reset)      (data_pipe_stream_t *);
    void                (*destroy)    (data_pipe_stream_t **);
    
};

DEFINE_INTERFACE (StreamIntercept) {
    
    stream_intercept_t *(*new)     (data_pipe_stream_t *);
    void                (*close)   (stream_intercept_t *);
    void                (*destroy) (stream_intercept_t **);
    
};

#endif
