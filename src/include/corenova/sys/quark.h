#ifndef __quark_H__
#define __quark_H__

#include <corenova/interface.h>

/** status flags **/
#define QUARK_STAT_RUNNING 0x8
#define QUARK_STAT_PAUSED  0x4

/** request flags **/
#define QUARK_REQ_STOP  0x8
#define QUARK_REQ_PAUSE 0x4
#define QUARK_REQ_ONCE  0x2
#define QUARK_REQ_LOOP  0
#define QUARK_MAX_NAMELEN 128

#if defined (linux)
# include <bits/local_lim.h>     /* for PTHREAD_STACK_MIN (about 16KB)*/
#else
# include <limits.h>
#endif

#ifndef PTHREAD_STACK_MIN
# define PTHREAD_STACK_MIN 16384
#endif

#define QUARK_DEFAULT_STACK_SIZE PTHREAD_STACK_MIN * 16 /* roughly about 256KB */

typedef boolean_t (*quark_func_t)(void *);

typedef struct {
	uint8_t stat:4;			/* quark status flags */
	uint8_t req :4;			/* quark request flags */
	
	pthread_t       life;
	pthread_cond_t  ready;
	pthread_cond_t  dead;
	pthread_cond_t  pause;
	pthread_cond_t  unpause;
	pthread_mutex_t lock;
    pthread_attr_t  attr;
	pthread_t	parent;

	char name[QUARK_MAX_NAMELEN];
	quark_func_t func;			/* core callback function */
	void *data; 				/* core user data */
} quark_t;

/*//////// MODULE DECLARATION //////////////////////////////////////////*/

DEFINE_INTERFACE (Quark)
{
	quark_t  *(*new)      (quark_func_t func, void *data);
	void      (*destroy)  (quark_t **);
	boolean_t (*spin)     (quark_t *);
	void      (*once)     (quark_t *);
	void      (*stop)     (quark_t *, uint32_t timeout_ms);
	void      (*pause)    (quark_t *);
	void      (*unpause)  (quark_t *);
	void      (*setname)  (quark_t *, char *);
	void      (*kill)     (quark_t *, int);
	void      (*killAll)  (void);
};

#endif 
