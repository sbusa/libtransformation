
#ifndef __queue_H__
#define __queue_H__

#include <corenova/interface.h>

typedef struct __queue_item {
  void *node;
  struct __queue_item *next;
} queue_item_t;

#define QUEUE_CHUNK_SIZE 8
#define QUEUE_MAX_CHUNKS 255
#define DEFAULT_QUEUE_TIMEOUT 100

/**
 * the dynamic queue that grows & shrinks, and manages the queue
 * behavior.
 **/
typedef struct {
	/* configurable vars */
	__u8  status  :3;			/* see: status */
	__u8  blocking:1;			/* on or off */
	__u8  rsv     :4;			/* reserved */

	boolean_t disabled;			/* set if disabled, prevents put (allows get, drop!) */
	
	uint32_t  chunk_size;       /* configured chunk size */
	unsigned char max_chunks;	/* configured max num chunks */
	uint32_t timeout;           /* get timeout if blocking */
    
	/* queue size / chunks */
	unsigned char num_chunks;	/* current number of chunks */
	queue_item_t *chunks[QUEUE_MAX_CHUNKS];

	/* thread safe controls */
	pthread_mutex_t lock; 
	pthread_cond_t  wait; 

	/* queue get/put access */
	queue_item_t *putpos;
	queue_item_t *getpos;

	uint32_t pending;           /* number of pending elements */
	uint32_t drops;             /* number of dropped elements */
} cqueue_t;

/*//////// MODULE DECLARATION //////////////////////////////////////////*/

DEFINE_INTERFACE (Queue)
{
	cqueue_t  *(*new)        (uint32_t chunk_size, unsigned char max_chunks);
	boolean_t (*put)         (cqueue_t *, void *);
	void     *(*get)         (cqueue_t *);
	void     *(*drop)        (cqueue_t *);
	void      (*setTimeout)  (cqueue_t *, __u32 t_ms);
	void      (*setBlocking) (cqueue_t *, boolean_t);
	void      (*disable)     (cqueue_t *);
	void      (*destroy)     (cqueue_t **);
};

#endif
