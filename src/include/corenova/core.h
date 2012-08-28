#ifndef __CORE_H__
#define __CORE_H__

#define _GNU_SOURCE

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if HAVE_LIBPTHREAD
# include <pthread.h>
static inline void __lock_cleanup (void *X) { pthread_mutex_unlock ((pthread_mutex_t *) X); }
# define MUTEX_TYPE       pthread_mutex_t
# define MUTEX_SETUP(X)   pthread_mutex_init (&(X), NULL)
# define MUTEX_CLEANUP(X) pthread_mutex_destroy (&(X))
# define MUTEX_LOCK(X)    pthread_mutex_lock (&(X))
# define MUTEX_UNLOCK(X)  pthread_mutex_unlock (&(X))
# define MUTEX_PUSH(X)    MUTEX_LOCK(X); pthread_cleanup_push(&__lock_cleanup,&(X))
# define MUTEX_POP(X)     pthread_cleanup_pop (1)
# define THREAD_ID        pthread_self ()
#else
# error "NO VALID PTHREAD FOUND!"
# define MUTEX_TYPE unsigned char
# define MUTEX_SETUP(X)
# define MUTEX_CLEANUP(X)
# define MUTEX_LOCK(X)
# define MUTEX_UNLOCK(X)
# define THREAD_ID 0
#endif

#if HAVE_LIBNETLOG
#  include <netlog.h>
#endif

#include <ltdl.h>				/* libtool dynamic loader library */
#include <stdio.h>				/* for printf */
#include <stdlib.h>				/* for getenv */
#include <string.h>				/* for strncmp */

#if defined (freebsd8) || defined (freebsd7) || defined (solaris2)

typedef int8_t __s8;
typedef uint8_t __u8;
typedef int16_t __s16;
typedef uint16_t __u16;
typedef int32_t __s32;
typedef uint32_t __u32;

#else

#include <asm/types.h>			/* for __u* types */

#endif

#if HAVE_STDINT_H

#include <stdint.h>

#else

typedef u_int8_t uint8_t;
typedef u_int16_t uint16_t;
typedef u_int32_t uint32_t;
typedef u_int64_t uint64_t;

#endif

#if defined (solaris2)

typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;

#endif

#include <sys/types.h>

#define ALIGNED64 __attribute__ ((aligned(8)))

#include "macros.h"

#define CONSTRUCTOR __attribute__ ((constructor))
#define DESTRUCTOR  __attribute__ ((destructor))

#ifndef TRUE
# define TRUE 1
#endif
#ifndef FALSE
# define FALSE 0
#endif

#if !defined (solaris2)

typedef unsigned char boolean_t;

#endif

typedef struct {
	char * key;
	char * val;
} keyval_t;

typedef struct {
	char *    str;
	uint16_t len;
} varchar_t;

#define VARCHAR(X,Y) X.str = strdup(Y); X.len = strlen(X.str);

#define LIST(args...) { args, 0 }

#define LT_SYMBOL2(PRE,POST) CONC3(PRE,_LTX_,POST)

#endif /* __CORE_H__ */

