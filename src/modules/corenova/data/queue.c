#include <corenova/source-stub.h>

THIS = {
	.version     = "2.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "This module implements data queue system.",
	.implements  = LIST ("Queue")
};

#include <corenova/data/queue.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <sys/time.h>			/* struct timeval */

static boolean_t
_growQueue (cqueue_t *q) {
	if (q->num_chunks != q->max_chunks) {
		__u16 growby = q->chunk_size;
		queue_item_t *chunk = (queue_item_t *)calloc(growby,sizeof(queue_item_t));
		queue_item_t *ptr   = chunk;
		if (chunk) {
//			DEBUGP(DINFO,"_growQueue","grow by %u elements.",growby);
			if (q->putpos && q->getpos) {
				ptr = chunk + (growby-1);
				ptr->next = q->putpos->next;
				q->putpos->next = chunk;
			} else {
				ptr = chunk + (growby-1);
				ptr->next = chunk;
				q->putpos = q->getpos = chunk;
			}
			while (--growby) {
				ptr = chunk + (growby-1);
				ptr->next = chunk + growby;
//				(&chunk)[growby-1]->next = (&chunk)[growby];
			}
			q->chunks[q->num_chunks] = chunk;
			q->num_chunks++;
			return TRUE;
		}
		/* only real failure if during initialization */
//		if (!q->getpos || !q->putpos) return FALSE;
	}
	return FALSE;
}

static void
_shrinkQueue (cqueue_t *q) {
	if (q->num_chunks > 1) {
		queue_item_t *save = q->chunks[q->num_chunks-1];
		queue_item_t *ptr = save + q->chunk_size - 1;
		ptr->next = save;		/* circle to self */
//		save[q->chunk_size-1]->next = save[0]; /* circle to self */
		while (--q->num_chunks) {
			free(q->chunks[q->num_chunks-1]);
		}
		q->num_chunks = 1;
		q->chunks[0] = save;
		q->putpos = q->getpos = save;
		//DEBUGP(DINFO,"_shrinkQueue","back to original size.");
	}
}

/*//////// INTERFACE ROUTINES //////////////////////////////////////////*/

static cqueue_t *
_new (uint32_t chunk_size, unsigned char max_chunks) {
	cqueue_t *q = (cqueue_t *)calloc(1,sizeof(cqueue_t));
	if (q) {
		q->chunk_size = (chunk_size>QUEUE_CHUNK_SIZE)?chunk_size:QUEUE_CHUNK_SIZE;
		q->max_chunks = (max_chunks>0)?max_chunks:1;
		pthread_mutex_init(&q->lock, NULL);
		pthread_cond_init (&q->wait, NULL);
		if (!_growQueue(q)) {
			free(q); return NULL;
		}
		//DEBUGP(DINFO,"new","chunk_size=%u, max_chunks=%u", chunk_size, max_chunks);
	}
	return q;
}

static void
_destroy (cqueue_t **queuePtr) {
	if (queuePtr) {
		cqueue_t *queue = *queuePtr;
		if (queue) {
			/*
			  flush all remaining elements in the queue
			  
			  WARNING: calling this will likely result in massive memory
			  leak!

			  Better to have specific application utilizing Queue to call
			  "disable", and then "drop" all remaining items *BEFORE*
			  calling this routine!
			*/
			while (queue->getpos != queue->putpos) {
				free (queue->getpos->node);
				queue->getpos = queue->getpos->next;
			} 
			while (queue->num_chunks) {
				free(queue->chunks[queue->num_chunks-1]);
				queue->num_chunks--;
			}
			free(queue);
			*queuePtr = NULL;
		}
	}
}

/*
  set methods
*/
static void
_setTimeout (cqueue_t *q, __u32 timeout_ms) {
    if (q) {
        q->timeout = timeout_ms;
    }
}

static void
_setBlocking (cqueue_t *q, boolean_t toggle) {
    DEBUGP (DDEBUG,"_setBlocking","setting this queue for blocking...");
    if (q)
        q->blocking = toggle;
}

/*
  data access methods
 */
static boolean_t
_put (cqueue_t *queue, void *in) {
	if (queue && !queue->disabled) {
        if (!in) {
            DEBUGP (DWARN,"_put","NULL data requested to be placed into the queue!");
            return TRUE;   /* pretend that the data went into the queue... */   
        }
        
        MUTEX_LOCK (queue->lock);
		if (queue->putpos->next == queue->getpos){
			if (!_growQueue(queue)) {
                MUTEX_UNLOCK (queue->lock);
				return FALSE;
			}
		}
		queue->putpos->node = in;
		queue->putpos = queue->putpos->next;
		queue->pending++;
        if (queue->blocking || queue->timeout > 0)
            pthread_cond_broadcast(&queue->wait);
        MUTEX_UNLOCK (queue->lock);
		return TRUE;
	}
	return FALSE;
}

static void *
_drop (cqueue_t *queue) {
	if (queue) {
		void *data = I (Queue)->get (queue);
		if (data) {
			queue->drops++;
		}
		return data;
	}
	return NULL;
}

static void *
_get (cqueue_t *queue) {
	void *data = NULL;
	if (queue) {
        MUTEX_PUSH (queue->lock);
		if (queue->putpos == queue->getpos) {
			_shrinkQueue(queue);
		}

        if (queue->getpos->node) {
            
			data = queue->getpos->node;
			queue->getpos->node = NULL;
			queue->getpos = queue->getpos->next;
			queue->pending--;
            
		} else if (!queue->disabled) {
            
            if (queue->blocking) {
                if (!pthread_cond_wait (&queue->wait,&queue->lock)) {
                    if (queue->getpos->node) {
                        data = queue->getpos->node;
                        queue->getpos->node = NULL;
                        queue->getpos = queue->getpos->next;
                        queue->pending--;
                    }
                }
            } else if (queue->timeout > 0) {
                struct timeval  now;
                struct timespec to;
                gettimeofday(&now,NULL);

                to.tv_sec = now.tv_sec + queue->timeout/1000;
                to.tv_nsec = (now.tv_usec * 1000) + (queue->timeout%1000 * 1000000 );
                if (to.tv_nsec >= 1000000000) {
                    to.tv_sec += 1;
                    to.tv_nsec %= 1000000000;
                }

                /* 
                 * DEBUGP (DDEBUG,"_get","waiting for %u ms: now is %lu.%lu wake at %lu.%lu",
                 *         queue->timeout, now.tv_sec, now.tv_usec * 1000, to.tv_sec, to.tv_nsec);
                 */
                if (!pthread_cond_timedwait(&queue->wait,&queue->lock,
                                            (const struct timespec *)&to)) {
                    if (queue->getpos->node) {
                        data = queue->getpos->node;
                        queue->getpos->node = NULL;
                        queue->getpos = queue->getpos->next;
                        queue->pending--;
                    }
                } else {
                    //DEBUGP (DDEBUG,"_get","pthread_cond_timedwait FAILED!");
                }
            }
		}
        MUTEX_POP (queue->lock);
	}
	return data;
}

static void _disable (cqueue_t *queue) {
	if (queue)
		queue->disabled = TRUE;
}

IMPLEMENT_INTERFACE (Queue) = {
	.new         = _new,
	.get         = _get,
	.put         = _put,
	.drop        = _drop,
	.setTimeout  = _setTimeout,
	.setBlocking = _setBlocking,
	.disable     = _disable,
	.destroy     = _destroy
};
	
