#include <corenova/source-stub.h>

THIS = {
	.version     = "2.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "This module manages threads.",
	.implements  = LIST ("Quark")
};

#include <corenova/sys/quark.h>

/*//////// HELPER ROUTINES //////////////////////////////////////////*/

#include <signal.h>             /* for pthread_kill */
#include <sys/time.h>			/* struct timeval */
#include <unistd.h>

static boolean_t killAllQuarks = FALSE;
extern int32_t QuarkCount;

static void _request_pause(quark_t *this)
{
	pthread_mutex_lock(&this->lock);
	pthread_cond_broadcast(&this->pause);
	pthread_cond_wait(&this->unpause, &this->lock); /* wait for unpause */
	pthread_mutex_unlock(&this->lock);
}

/*
  the CORE quark execution routine
*/
static void *quark_core(void *arg)
{
	quark_t *this = (quark_t *)arg;
	pthread_mutex_lock(&this->lock);
	this->stat |= QUARK_STAT_RUNNING;
	pthread_cond_broadcast(&this->ready);
	pthread_mutex_unlock(&this->lock);

#ifdef DEBUG_INTENSIVE    
    {
        /* the following will display actual stack usage size! */
        char cmd[1024];
        FILE *fp;
        fprintf (stderr, "&arg = %p\n", &arg);
        sprintf (cmd, "cat /proc/%d/maps", getpid ());
        fp = popen (cmd, "r");
        while (fgets (cmd, sizeof (cmd), fp)) {

            void *low, *hi;
            int n = sscanf (cmd, "%x-%x", &low, &hi);
            if (n) {

                if (low < (void*)&arg && (void*)&arg < hi) {

                    fprintf (stderr, "in region [%p, %p) (%d)\n", low, hi, (char*)hi - (char*)low);
                    fclose (fp);
                    break;
                }
            } 
        }
    }
#endif

    pthread_detach(pthread_self());

	while (!(this->req & QUARK_REQ_STOP) && !killAllQuarks && !SystemExit){
		if (!(*this->func)(this->data))
			break;
		if (this->req & QUARK_REQ_PAUSE) _request_pause(this);
		if (this->req & QUARK_REQ_ONCE) break;
	}
	
	pthread_mutex_lock(&this->lock);
	this->stat &= !QUARK_STAT_RUNNING;
	pthread_cond_broadcast(&this->dead);
	pthread_mutex_unlock(&this->lock);

	DEBUGP (DDEBUG,"quarkCore","exiting '%s' %lX...", this->name, (unsigned long) pthread_self ());
	    
//	pthread_exit(&this->life);

	return NULL;
}

/*//////// API ROUTINES //////////////////////////////////////////*/

static quark_t *
_new (quark_func_t func, void *data) {
	quark_t *quark = (quark_t *)calloc(1,sizeof(quark_t));
	
	if (quark) {
		quark->parent = pthread_self();

        pthread_cond_init(&quark->ready,NULL);
		pthread_cond_init(&quark->dead,NULL);
		pthread_cond_init(&quark->pause,NULL);
		pthread_cond_init(&quark->unpause,NULL);
		pthread_mutex_init(&quark->lock,NULL);
//        pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
        pthread_attr_init (&quark->attr);
        if (pthread_attr_setstacksize (&quark->attr,QUARK_DEFAULT_STACK_SIZE) != 0) {
            DEBUGP (DERR,"newQuark","unable to set the stack size to %d",QUARK_DEFAULT_STACK_SIZE);
        }
		quark->func = func;
		quark->data = data;
		strncpy(quark->name, "some quark", QUARK_MAX_NAMELEN);
		QuarkCount++;
	}
	return quark;
}

static void
_kill (quark_t *quark, int signo) {
    if(quark && quark->life)
        pthread_kill(quark->life, signo);
}

static boolean_t
_spin (quark_t *quark) {
	if (quark->stat & QUARK_STAT_RUNNING) return TRUE;
	pthread_mutex_lock(&quark->lock);
	if (pthread_create(&quark->life, &quark->attr, quark_core, (void *)quark) == 0){
		DEBUGP (DDEBUG,"spin","'%s' off and away!", quark->name);
		pthread_cond_wait(&quark->ready,&quark->lock);
		pthread_mutex_unlock(&quark->lock);
		return TRUE;
	} else {
		DEBUGP(DERR,"spin","pthread_create error!  investigation required!");
		pthread_mutex_unlock (&quark->lock);
	}
	return FALSE;
}

static void
_setname (quark_t *quark, char *name) {

    if(quark && name) {
        strncpy(quark->name, name, QUARK_MAX_NAMELEN);
    }

}

static void
_once (quark_t *quark) {
	quark->req |= QUARK_REQ_ONCE;
	I (Quark)->spin(quark);
}

static void
_stop (quark_t *quark, uint32_t timeout_ms) {
	pthread_mutex_lock(&quark->lock);
	if (quark->stat & QUARK_STAT_RUNNING) {
        struct timeval  now;
        struct timespec to;
        gettimeofday(&now,NULL);

        to.tv_sec = now.tv_sec + timeout_ms/1000;
        to.tv_nsec = (now.tv_usec * 1000) + (timeout_ms%1000 * 1000000 );
        if (to.tv_nsec >= 1000000000) {
            to.tv_sec += 1;
            to.tv_nsec %= 1000000000;
        }
        
		quark->req |= QUARK_REQ_STOP;
		if (pthread_cond_timedwait(&quark->dead,&quark->lock,
                                   (const struct timespec *)&to) != 0) {
            DEBUGP (DDEBUG,"stop","stoping the quark '%s'",quark->name);
            to.tv_nsec += 1000000;
            if (to.tv_nsec >= 1000000000) {
                to.tv_sec += 1;
                to.tv_nsec %= 1000000000;
            }
            if (pthread_cond_timedwait(&quark->dead,&quark->lock,
                                       (const struct timespec *)&to) != 0) {
                DEBUGP (DDEBUG,"stop","forcefully terminating the quark '%s'",quark->name);
                pthread_cancel (quark->life);
            }
            quark->stat &= !QUARK_STAT_RUNNING;
        }
	}
	pthread_mutex_unlock(&quark->lock);
}

static void
_pause (quark_t *quark) {
	pthread_mutex_lock(&quark->lock);
	if (quark->stat & QUARK_STAT_RUNNING){
		quark->req |= QUARK_REQ_PAUSE;
		pthread_cond_wait(&quark->pause, &quark->lock);
		quark->req = 0;
		quark->stat |= QUARK_STAT_PAUSED;
	}
	pthread_mutex_unlock(&quark->lock);
}

static void 
_unpause (quark_t *quark) {
	pthread_mutex_lock(&quark->lock);
	if (quark->stat & QUARK_STAT_PAUSED){
		quark->req = 0;
		pthread_cond_broadcast(&quark->unpause);
	}
	pthread_mutex_unlock(&quark->lock);
}

static void
_destroy (quark_t **qPtr) {
	if (qPtr) {
		quark_t *quark = *qPtr;
		if (quark) {
            DEBUGP (DDEBUG,"destroy","destroying quark '%s'",quark->name);
			if (quark->stat & QUARK_STAT_RUNNING)
				I (Quark)->stop(quark,0);
            
            pthread_mutex_destroy (&quark->lock);
            pthread_cond_destroy  (&quark->ready);
            pthread_cond_destroy  (&quark->dead);
            pthread_cond_destroy  (&quark->pause);
            pthread_cond_destroy  (&quark->unpause);
            pthread_attr_destroy  (&quark->attr);

            DEBUGP (DDEBUG,"destroy","quark '%s' destroyed",quark->name);
            QuarkCount--;
			free(quark);
			*qPtr = NULL;
		}
	}
}

static void setKillAllQuarks () {
	killAllQuarks = TRUE;
}

IMPLEMENT_INTERFACE (Quark) = {
	.new     = _new,
	.destroy = _destroy,
	.spin    = _spin,
	.once    = _once,
	.stop    = _stop,
	.pause   = _pause,
	.unpause = _unpause,
	.setname = _setname,
	.kill    = _kill,
	.killAll = setKillAllQuarks
};
