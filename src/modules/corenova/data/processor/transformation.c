#include <corenova/source-stub.h>

THIS = {
	.version     = "3.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "This module acts as a powerful transformation data processor dealing with transformation logic",
	.implements  = LIST ("DataProcessor","TransformationProcessor","Transformation","TransformTrace", "TransformCounter"),
	.requires    = LIST ("corenova.data.configuration",
                         "corenova.data.configuration.xform",
                         "corenova.data.array",
						 "corenova.data.string",
                         "corenova.data.md5",
                         "corenova.data.queue",
                         "corenova.data.cache",
						 "corenova.sys.loader",
                         "corenova.sys.transform",
						 "corenova.sys.quark",
						 "corenova.data.parser.jsonc"),
    .transforms  = LIST ("* => transform:back", /* direct ONLY match */
                         "* => transform:feeder", /* direct ONLY match */
                         "transform:back -> *",
                         "* -> transform:counter",
                         "transform:counter -> data:object::json")
};

#include <corenova/data/processor/transformation.h>
#include <corenova/data/configuration/xform.h>
#include <corenova/data/string.h>
#include <corenova/data/md5.h>
#include <corenova/data/queue.h>
#include <corenova/sys/loader.h>
#include <corenova/data/parser/jsonc.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <unistd.h>             /* for usleep */
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>

#ifdef HAVE_SYS_LOADAVG_H
# include <sys/loadavg.h>
#endif

#ifndef HAVE_GETLOADAVG
#include <corenova/data/file.h>

static int getloadavg (double d[], int l) {
    FILE *f;
    if (!(f = fopen ("/proc/loadavg", "r")))
        return -1;
    if (fscanf (f, "%lf %lf %lf", &d[0], &d[1], &d[2]) != 3) {
        fclose (f);
        return -1;
    }
    fclose (f);
    return 3;
}
#endif

/*
 * linker entry cache definitions
 */

typedef struct {

    char *key;
    array_t *targets;
    
} linker_entry_t;

static inline int linker_entry_cmp (void *key, void *data) {
    char *A = (char *)key;
    char *B = ((linker_entry_t *)data)->key;
    DEBUGP (DDEBUG,"linker_entry_cmp","comparing %s = %s",A,B);
    return strcmp (A,B);
}

static inline int linker_wild_entry_cmp (void *key, void *data) {
    char *A = (char *)key;
    char *B = ((linker_entry_t *)data)->key;
    DEBUGP (DDEBUG,"linker_wild_entry_cmp","comparing %s = %s",A,B);
    if (I (String)->equalWild (A,B))
        return 0;
    return strcmp (A,B);
}

static inline void linker_entry_del (void *data, void *cookie) {
    linker_entry_t *entry = (linker_entry_t *) data;
    if (entry) {
        DEBUGP (DDEBUG,"linker_entry_del","removing %s",entry->key);
        I (Array)->destroy (&entry->targets,NULL);
        free (entry);
    }
}

#define _INIT 1
#define _CLEAN 2
#define _RELOAD 3
/* stamp info of /proc/self/fd/ never change after created */
static int fdctl(uint32_t *fdbitmap, int cmd)
{
	char filename[64];
	DIR *dir;
	struct dirent *entry;
	int res;

	snprintf(filename, 64, "/proc/self/fd");
	dir = opendir (filename);
	if (dir) {
		if (cmd == _INIT) memset(fdbitmap, 0, FD_MAP_MAX / 8);

		while ((entry = readdir (dir)) != NULL) {
			int fd;
			mode_t mode;
			struct stat type;

			fd = atoi(entry->d_name);
			snprintf(filename, 64, "/proc/self/fd/%d", fd);
			if (fd > 4 && stat(filename, &type) == 0) {
				mode = type.st_mode & S_IFMT;
				switch (mode) {
					case S_IFIFO: /* redirect to debug.err & debug.out */
						break;

					case S_IFREG:
						if (fd > FD_MAP_MAX) {
							DEBUGP (DERR, "fdctl", "fd %d > FD_MAP_MAX %d", fd, FD_MAP_MAX);
							break;
						}
						
						if (cmd == _INIT) {
							fdbitmap[ fd / 32 ] |= 1 << (fd % 32);
							break;
						}
						
						if (fdbitmap[ fd / 32 ] & (1 << (fd % 32))) {
							if (cmd == _CLEAN) break;
						} else {
							fdbitmap[ fd / 32 ] |= 1 << (fd % 32);
							if (cmd == _RELOAD) break;
						}		
						
						do {
							res = close(fd);
							DEBUGP (DWARN, "fdctl", "fd %d %d %d", fd, res, errno);
						} while (res == -1 && (errno == EINTR || errno == EIO));
						fdbitmap[ fd / 32 ] &= ~(1 << (fd % 32));
	
						break;

					case S_IFSOCK:
						if (cmd == _RELOAD) {
							do {
								res = close(fd);
								DEBUGP (DWARN, "fdctl", "fd %d %d %d", fd, res, errno);
							} while (res == -1 && (errno == EINTR || errno == EIO));
						}

						break;

					default:
						DEBUGP (DWARN, "fdctl", "unknow file mode %d!", mode); 
				}
			}
		}
		
		closedir (dir);
	}

	return 0;
}

static char *
TransformCounterToJson (transform_counter_t *counter) {
	char *result = NULL;

	if(!counter)
		return NULL;

	json_object *root = I(jsonc)->newObject(JSON_OBJECT);
	if (root) {

		I(jsonc)->addObject(root, JSON_STRING, "format", counter->format);
		I(jsonc)->addObject(root, JSON_INT, "count", &counter->count);
		I(jsonc)->addObject(root, JSON_DOUBLE, "start", &counter->start);
		I(jsonc)->addObject(root, JSON_DOUBLE, "duration", &counter->duration);

		result = I(jsonc)->toString(root);

		I(jsonc)->destroyObject(root);

		if(result) {
			return result;
		}

	}

	return NULL;
}

static void 
destroyTransformCounter(transform_counter_t **counterPtr) {

	if(counterPtr) {
		transform_counter_t *counter = *counterPtr;
		if (counter) {

			if(counter->format)
				free(counter->format);

			free(counter);
			*counterPtr = NULL;
		}
	}

}

/*//////// Transform Counter Interface Implementation //////////////////////////////////////////*/
IMPLEMENT_INTERFACE (TransformCounter) = {
    .toJson  = TransformCounterToJson,
    .destroy = destroyTransformCounter
};


/*//////// Transformation Interface Implementation //////////////////////////////////////////*/

/*
 * This is a fundamental transformation provided by the processor to
 * enable LOOP operation for continuous processing.
 *
 * Basically, transform:feeder -> xxxxx is how a data is provided.
 * Now, the reverse of xxxxx -> transform:feeder is the necessary
 * instruction to repeat the operation by returning a feeder token for
 * additional input.
 *
 * The processor begins by SEEDING the system with initial feeder
 * tokens.  Feeder tokens are tokens that contain the
 * transform_object_t which holds "transform:feeder" data type.  The
 * content of that object is the transform_target_t object.
 *
 * This way, when the transformation rule specifies that it is time to
 * return the "transform:feeder" data type, the processor is able to
 * go back to the original transform:feeder -> xxxxx operation that
 * allowed it to retrieve the input.
 *
 * If the type of transformation desired is a one-time operation, all
 * you have to do is NOT return the transform feeder data back to the
 * system. :)
 */
TRANSFORM_EXEC (any2transformback) {
    return I (TransformObject)->new ("transform:back",NULL);
}

TRANSFORM_EXEC (feederback) {
//    MUTEX_LOCK (in->lock);
    MODULE_LOCK ();
    transform_object_t *pop = I (TransformObject)->pop (in,"transform:feeder");
    MODULE_UNLOCK ();
//    MUTEX_UNLOCK (in->lock);
    return pop;
}

TRANSFORM_EXEC (transformback2any) {
//    MUTEX_LOCK (in->lock);
    MODULE_LOCK ();
    transform_object_t *pop = I (TransformObject)->pop (in,xform->to);
    MODULE_UNLOCK ();
//    MUTEX_UNLOCK (in->lock);
    return pop;
}

TRANSFORM_EXEC(any2transformcounter) {
	struct timeval current_tv;
	unsigned long elapsed_sec;

	transform_counter_controller_t *in_counter = (transform_counter_controller_t *)xform->instance;
	unsigned long timeout_in_sec = in_counter->interval;

	if (in_counter->start_time.tv_sec == 0) {
		gettimeofday(&in_counter->start_time, NULL);
	}

	gettimeofday(&current_tv, NULL);
	DEBUGP (DDEBUG, "any2transformcounter", ": seconds %lu\n", current_tv.tv_sec);

	elapsed_sec = ((current_tv.tv_sec - in_counter->start_time.tv_sec) + 
			((current_tv.tv_usec -  in_counter->start_time.tv_usec)/1000000));

	if (elapsed_sec > timeout_in_sec) {

		DEBUGP(DDEBUG, "feeder2counter", "transform:counter");
		//update the transform object that's sent to logger service and create a transform object with it
		transform_counter_t *out_counter_p = (transform_counter_t *)calloc (1,sizeof (transform_counter_t));

		if (in_counter && out_counter_p) {

			out_counter_p->format = strdup(in_counter->format);
			out_counter_p->count = in_counter->count;
			out_counter_p->start = (in_counter->start_time.tv_sec + ((in_counter->start_time.tv_usec)/1000000)); 
			/* Duration calculated based on elapsed time as above gives the accurate time spent since the last
			 * transform counter call might have more time which would have exceeded the configured timeout.
			 * Hence the duration updated and sent to logger service is accurate */
			out_counter_p->duration = elapsed_sec;

			/* Initialise in_counter start_time and count to zero */
			memset (&in_counter->start_time, 0, sizeof(in_counter->start_time));
			in_counter->count = 0;

			transform_object_t *obj = I(TransformObject)->new("transform:counter", out_counter_p);
			if (obj) {
				obj->destroy = (XDESTROY) I(TransformCounter)->destroy;
				return obj;
			}
			I(TransformCounter)->destroy(&out_counter_p);
		}

	} else {
		// increment counter in instance object
		in_counter->count++;
	}   
	return NULL;
}

TRANSFORM_EXEC(transformcounter2jsonObject) {
	char *json_p = NULL;

	/* Conversion of transform:counter into data:object::json */
	if(in && in->data) {
		DEBUGP (DDEBUG,"transformcounter2jsonObject","called with in: %p in->data: %p", in, in->data);

		transform_counter_t *counter = (transform_counter_t *) in->data;

		if(counter) {
			json_p = I(TransformCounter)->toJson(counter);

			if(json_p) {
				transform_object_t *obj = I(TransformObject)->new("data:object::json", json_p);
				return obj;
			}
		}
	}

	return NULL;
}

TRANSFORM_NEW (newEngineTransformation) {

	TRANSFORM ("*","transform:back", any2transformback);
	TRANSFORM ("*","transform:feeder", feederback);
	TRANSFORM ("transform:back","*", transformback2any);
	TRANSFORM ("*","transform:counter", any2transformcounter);
	TRANSFORM ("transform:counter", "data:object::json", transformcounter2jsonObject);

	IF_TRANSFORM(any2transformcounter) {

		TRANSFORM_HAS_PARAM ("transform_counter_interval");

		transform_counter_controller_t *counter_controller = (transform_counter_controller_t *)calloc (1,sizeof (transform_counter_controller_t));
		if (counter_controller) {
		    /*Timeout in seconds */	
			counter_controller->interval =(unsigned long)(I(Parameters)->getTimeValue(blueprint, "transform_counter_interval")); 
			counter_controller->format =(char *)(I(Parameters)->getValue(blueprint, "transform_counter_name")); 
		} 
		TRANSFORM_WITH(counter_controller);
	}

} TRANSFORM_NEW_FINALIZE;

TRANSFORM_DESTROY (destroyEngineTransformation) {
	IF_TRANSFORM(any2transformcounter) {
		if (xform->instance) {
			transform_counter_controller_t *counter_controller = (transform_counter_controller_t *)xform->instance;
			if(counter_controller->format) free (counter_controller->format);
			free (counter_controller);
		}    
	}    
} TRANSFORM_DESTROY_FINALIZE;

IMPLEMENT_INTERFACE (Transformation) = {
    .new     = newEngineTransformation,
    .destroy = destroyEngineTransformation,
    .execute = NULL,
    .free    = NULL
};

/*//////// HELPER ROUTINES //////////////////////////////////////////*/

DEFINE_INTERFACE (TransformTrace) {
    void (*log) (transformation_profiler_t *profiler, transformation_t *transform, char *log);
};

/* 
 * typedef array_t transform_trace_t;
 * 
 * typedef struct {
 * 
 *     file_t *file;
 *     transformation_t *transform;
 *     
 * } transform_trace_t;
 * 
 * 
 * static transform_trace_t *
 * _newTracer (transformation_profiler_t *profiler, transformation_t *transform) {
 *     if (profiler && transform) {
 *         transform_trace_t *trace = (transform_trace_t *)calloc (1,sizeof (transform_trace_t));
 *         if (trace) {
 *             trace->file = profiler;
 *             trace->transform = transform;
 *         }
 *         return trace;
 *     }
 *     return NULL;
 * }
 */

static void
_logTrace (transformation_profiler_t *profiler, transformation_t *transform, char *log) {
    if (profiler && transform && log) {
        char *message = I (String)->new ("%lu.%06lu %X [%s -> %s] %s\n",
                                         __tsec (), __tusec (), (uint32_t)pthread_self (), transform->from, transform->to, log);
        if (!I (File)->write (profiler,message,strlen (message),1)) {
            DEBUGP (DWARN,"_logTrace","unable to write %s into logfile!",message);
        }
        free (message);
    }
}

IMPLEMENT_INTERFACE (TransformTrace) = {
    .log = _logTrace
};

static boolean_t
_xformFeeder (void *inData) {
	transformation_processor_t *instance = (transformation_processor_t *)inData;
	if (instance) {
        unsigned long numProcessed = 0;
        transform_token_queue_t *feeder = instance->feederQueue;
        transform_token_t *token = NULL;

        while ((token = I (TransformTokenQueue)->get (feeder))) {
            while (!I (TransformTokenQueue)->put (instance->execQueue,token)) {
                usleep (1000); /* we don't like dropping stuff! */
            }
            numProcessed++;
        }

        if (numProcessed) {
            DEBUGP (DDEBUG,"_xformFeeder","pumped %lu tokens back into exec queue", numProcessed);
        } else {
            /* we may have a problem here...
             *
             * no feeder tokens have been returned by the executors, which could mean one of 2 things:
             *  1. all executors are stuck on processing feeder tokens (i.e. waiting for input)
             *  2. all executors are stuck on processing exec tokens and feeders are not getting any attention
             *
             * case #1 is perfectly fine.  case #2 is the problem
             *
             * need to detect for case #2 and dynamically create additional executors!
             */
            I (TransformationProcessor)->optimize (instance);
        }

        /* XXX - should implement default usleep if no timeout is defined? */
    }

    return TRUE;
}

static boolean_t
_xformExec (void *inData) {
    unsigned long numProcessed = 0;
	transformation_processor_t *instance = (transformation_processor_t *)inData;
	if (instance) {
        transform_token_queue_t *exec = instance->execQueue;
        transform_token_t *token = NULL;
        MUTEX_LOCK (instance->lock);
        instance->waitingExecutors++;
        MUTEX_UNLOCK (instance->lock);
        while ((token = I (TransformTokenQueue)->get (exec))) {
            uint16_t numLinker = 0, numDropped = 0;
            transformation_t *transform = NULL;

            numProcessed = 0;
            
          exec_begin:
            /* lookup transformation from matrix based on token target */
            /* profile the execution of this task */
            transform = I (TransformationMatrix)->lookup (instance->matrix, token->target);
            if (transform) {
                transform_object_t *tobj = NULL;
//                transform_trace_t *trace = I (TransformTrace)->new (instance->profiler,transform);

                MUTEX_LOCK (instance->lock);
                instance->waitingExecutors--;
                MUTEX_UNLOCK (instance->lock);
                I (TransformTrace)->log (instance->profiler,transform,"exec");
                if (transform->exec) {
                    DEBUGP (DDEBUG,"_xformExec","calling transform->exec for [%s -> %s] @ %p",transform->from,transform->to, transform->exec);
                    tobj = (*transform->exec) (transform, token->obj);
                }
                else if (I_ACCESS (transform->module,Transformation)->execute) {
                    DEBUGP (DDEBUG,"_xformExec","calling Transformation->execute for [%s -> %s] @ %p",transform->from,transform->to, I_ACCESS (transform->module,Transformation)->execute);
                    tobj = I_ACCESS (transform->module,Transformation)->execute (transform, token->obj);
                }
                MUTEX_LOCK (instance->lock);
                instance->waitingExecutors++;
                MUTEX_UNLOCK (instance->lock);
                
                if (tobj) {
                    I (TransformTrace)->log (instance->profiler,transform,"next");
                    /*
                     * first handle BUILT-IN transformation
                     */
                    if (transform->module == SELF) {
                        transformation_t *xform = transform;
                        IF_TRANSFORM (feederback) {
                            DEBUGP (DDEBUG,"_xformExec","returning FEEDER!");
                            // let's re-use the token :)
                            I (TransformObject)->destroy (&token->obj);
                            token->target = (transform_target_t *) tobj->data;
                            token->obj = tobj;

                            // HACK optimization
                            while (! I (TransformTokenQueue)->put (instance->feederQueue,token)) {
                                usleep (1000);
                            }

                            /* 
                             * while (! I (TransformTokenQueue)->put (instance->feederQueue,token)) {
                             *     usleep (500);
                             * }
                             */
                            MUTEX_LOCK (instance->lock);
                            instance->stat.numFeeder++;
                            MUTEX_UNLOCK (instance->lock);
                            I (TransformTrace)->log (instance->profiler,transform,"end");
                            continue;
                        }

                        IF_TRANSFORM (transformback2any) {
                            I (TransformObject)->destroy (&token->obj);
                        }
                    }

                    /*
                     * create attachment - a chain of transform objects for history & access
                     * NOTE: only if the new TOBJ does not have an override originator set!
                     */
                    if (!tobj->originator)
                        I (TransformObject)->attach (tobj, token->obj);

                    /*
                     * find out the coordinates that require
                     * execution and pop those into the exec
                     * queue
                     */
                    list_t *nextTargets = I (TransformationMatrix)->getNextTargets (instance->matrix, token->target);
                    if (nextTargets) {
                        int count = I (List)->count (nextTargets);
                        if (!count) { // end of chain!
                            /*
                             * end of the chain but we have data???
                             * let's see if we can link this to a different matrix location.
                             *
                             * optimized to use Cache to perform explict lookup match
                             */
                            I (TransformTrace)->log (instance->profiler,transform,"link");
                            if (instance->useCache) {
                                linker_entry_t *match = (linker_entry_t *) I (Cache)->get (instance->linkers.explicit, tobj->format);
                                if (match) {
                                    int i = 0;
                                    transform_target_t *target = NULL;
                                    while ((target = I (Array)->get (match->targets,i++))) {
                                        DEBUGP (DDEBUG,"_xformExec","adding 'explicit' linker from (%s) -> (%s)",tobj->format,match->key);
                                        I (List)->insert (nextTargets,I (ListItem)->new (target));
                                        numLinker++;
                                    }
                                } else {
                                    match = (linker_entry_t *) I (Cache)->get (instance->linkers.wild, tobj->format);
                                    if (match) {
                                        transform_target_t *target = NULL;
                                        if (I (Array)->count (match->targets) > 1) {
                                            int i = 0, best = 0, longest = 0;
                                            char *using = NULL;
                                            DEBUGP (DDEBUG,"_xformExec",
                                                    "found 'wildcard' linker match from (%s) -> (%s) but looks like there may be multiple targets for the key. "
                                                    "Check xform logic if this is not desired.",
                                                    tobj->format, match->key);
                                            while ((target = I (Array)->get (match->targets, i))) {
                                                transformation_t *xform = I (TransformationMatrix)->lookup (instance->matrix, target);
                                                if (xform && I (String)->equalWild (tobj->format,xform->from) && (strlen (xform->from) > longest)) {
                                                    longest = strlen (xform->from);
                                                    best = i;
                                                    using = xform->from;
                                                }
                                                i++;
                                            }
                                            target = I (Array)->get (match->targets, best);
                                            DEBUGP (DDEBUG,"_xformExec","adding 'wildcard' linker from (%s) -> (%s)",tobj->format,using);
                                        
                                        } else {
                                            target = I (Array)->get (match->targets, 0);
                                            DEBUGP (DDEBUG,"_xformExec","adding 'wildcard' linker from (%s) -> (%s)",tobj->format,match->key);
                                        }
                                        I (List)->insert (nextTargets,I (ListItem)->new (target));
                                        numLinker++;
                                    }
                                }
                            } else { /* old linear algorithm, does NOT require locks */
                                list_t *linkerTargets = instance->linkers.all;
                                list_item_t *item = I (List)->first (linkerTargets);
                                while (item) {

                                    transform_target_t *target = (transform_target_t *)item->data;
                                    transformation_t *xform = I (TransformationMatrix)->lookup (instance->matrix, target);
                                    if (xform) {

                                        if (I (String)->equal (tobj->format,xform->from)) {

                                            DEBUGP (DDEBUG,"_xformExec","adding 'explicit' linker from (%s) -> (%s)",tobj->format,xform->from);
                                            I (List)->insert (nextTargets,I (ListItem)->new (target));
                                            instance->stat.numLinker++;
                                        }
                                    }
                                    item = I (List)->next (item);
                                }

                                if (!I (List)->count (nextTargets)) {
                                    // still nothing?
                                    item = I (List)->first (linkerTargets);
                                    while (item) {

                                        transform_target_t *target = (transform_target_t *)item->data;
                                        transformation_t *xform = I (TransformationMatrix)->lookup (instance->matrix, target);
                                        if (xform) {

                                            if (I (String)->equalWild (tobj->format,xform->from)) {

                                                if (!I (List)->count (nextTargets)) {

                                                    DEBUGP (DDEBUG,"_xformExec","adding 'wildcard' linker from (%s) -> (%s)",tobj->format,xform->from);
                                                    I (List)->insert (nextTargets,I (ListItem)->new (target));
                                                    instance->stat.numLinker++;
                                                } else {

                                                    list_item_t *helpitem = I (List)->first (nextTargets);
                                                    if (helpitem && helpitem->data) {

                                                        transform_target_t *lastTarget = (transform_target_t *)helpitem->data;
                                                        transformation_t *lastXform    = I (TransformationMatrix)->lookup (instance->matrix,lastTarget);
                                                        if (lastXform) {

                                                            DEBUGP (DDEBUG,"_xformExec",
                                                                    "found 'wildcard' linker match from (%s) -> (%s) but has already matched (%s) -> (%s) before. "
                                                                    "Check xform logic if this is not desired.",
                                                                    tobj->format, xform->from, tobj->format, lastXform->from);
                                                        }
                                                    }
                                                    break;
                                                }
                                            }
                                        }
                                        item = I (List)->next (item);
                                    }
                                }
                            }
                            count = I (List)->count (nextTargets);
                        }

                        
                        if (count) {
                            list_item_t *item = NULL;
                            I (TransformTrace)->log (instance->profiler,transform,"split");
                            if (instance->useAtomic || count > 1) {
                                DEBUGP (DDEBUG,"_xformExec","splitting transform (%s) into %d copies",tobj->format,count);
                                tobj = I (TransformObject)->split (tobj,count);
                                while ((item = I (List)->pop (nextTargets))) {
                                    transform_target_t *target = (transform_target_t *)item->data;
                                    transform_token_t *newToken = I (TransformToken)->new (target,tobj);
                                    if (newToken) {
                                        DEBUGP (DDEBUG,"_xformExec","adding new token for transform (%s) to (%d,%d,%d)",tobj->format,target->set,target->rule,target->chain);
                                        while (! I (TransformTokenQueue)->put (instance->execQueue,newToken)) {
                                            usleep (1000);
                                        }
                                    } else {
                                        // should never happen, but what the hell...
                                        I (TransformObject)->destroy (&tobj);
                                    }
                                    I (ListItem)->destroy (&item);
                                }
                            } else { /* break atomic execution, but can help reduce stress on threads */
                                list_item_t *item = I (List)->pop (nextTargets);
                                transform_target_t *target = (transform_target_t *)item->data;
                                
                                I (ListItem)->destroy (&item);
                                
                                // let's re-use the token :)
                                token->target = target;
                                token->obj = tobj;
                                numProcessed++;

                                // protect the case where system halt is in order!
                                if (!SystemExit) {
                                    I (List)->destroy (&nextTargets);
                                    goto exec_begin;
                                }
                            }
                        } else {
                            DEBUGP (DDEBUG,"_xformExec","no further execution possible, dropping transform object (%s)!",tobj->format);
                            numDropped++;
                            I (TransformObject)->destroy (&tobj);
                        }
                        
                        I (List)->destroy (&nextTargets);
                    } else {
                        DEBUGP (DERR,"_xformExec","unable to retrieve nextTargets array!");
                        // XXX - must be out of memory!!!
                        I (TransformObject)->destroy (&tobj);
                    }
                } else {
                    // didn't produce anything... dump this token object.
                    DEBUGP (DDEBUG,"_xformExec","no new transform object produced! dropping transform object (%s)!",token->obj->format);
                    I (TransformTrace)->log (instance->profiler,transform,"destroy");
                    I (TransformObject)->destroy (&token->obj);
                }
                I (TransformTrace)->log (instance->profiler,transform,"end");
            } else {
                // XXX - what to do if transform cannot be resolved??????
            }

            I (TransformToken)->destroy (&token);
            numProcessed++;

            MUTEX_LOCK (instance->lock);
            instance->stat.numLinker += numLinker;
            instance->stat.numDropped += numDropped;
            instance->stat.numProcessed += numProcessed;
            MUTEX_UNLOCK (instance->lock);
        }
        MUTEX_LOCK (instance->lock);
        instance->waitingExecutors--;
        MUTEX_UNLOCK (instance->lock);
        
        if (!numProcessed) {
            /* we may have a problem here...
             *
             * there are no exec tokens (meaning all the rest of the executors are tied up on feeder tokens)
             *
             * if so, it is perfect opportunity to reset the number of executors to an optimal minimum.
             *
             * 10/29/2010 - plee
             *
             * we care about how many pool of minimum threads are in 'waiting' state.
             */
            if ((instance->waitingExecutors >= instance->minExecutors) || // we're very IDLE
                (instance->waitingExecutors == 0 &&  // we're very BUSY
                 instance->currentLoad > instance->maxLoad)) {
                instance->stat.numExecutorExit++;
                return FALSE;
            }
            // only usleep if set to timeout?
            usleep (500);
        }
    }
    return TRUE;
}

static uint32_t numOptimize = 0;

static boolean_t
_xformMonitor (void *inData) {
	transformation_processor_t *instance = (transformation_processor_t *)inData;
	if (instance) {
        double load[3];
        if (instance->maxLoad && (getloadavg (load,1) > 0)) 
            instance->currentLoad = (uint16_t) (load[0] * 100);

        // always try to optimize at least once a second
        I (TransformationProcessor)->optimize (instance);

        /* it's not time yet, let's see what has happened since */
        // count active executors
        int activeExecutors = 0, deadExecutors = 0;
        int i = 0;
        quark_t *executor = NULL;
        for (i = 0; (executor = (quark_t *) I (Array)->get (instance->executors,i)); i++) {
            if (executor->stat & QUARK_STAT_RUNNING) {
                activeExecutors++;
            } else {
                deadExecutors++;
            }
        }
        if (activeExecutors > instance->stat.maxExecutors)
            instance->stat.maxExecutors = activeExecutors;

        /* print out some statistics every x interval */
        if (instance->statInterval) {
            long int tsec = __tsec ();
            if ( tsec - instance->lastStat >= (instance->statInterval / 1000) ) {
                    
                DEBUGP (DWARN,"stat","------------ TRANSFORMATION STAT ------------");
                DEBUGP (DWARN,"stat","| secs since last stat         = %ld", tsec - instance->lastStat);
                DEBUGP (DWARN,"stat","| current system load          = %f", (float) instance->currentLoad/100);
                DEBUGP (DWARN,"stat","| current active executors     = %d", activeExecutors);
                DEBUGP (DWARN,"stat","| current waiting executors    = %d", instance->waitingExecutors);
                DEBUGP (DWARN,"stat","| current dead executors       = %d", deadExecutors);
                DEBUGP (DWARN,"stat","| highest num executors        = %d", instance->stat.maxExecutors);
                DEBUGP (DWARN,"stat","| highest pending exec tokens  = %d", instance->stat.maxPendingExec);
                DEBUGP (DWARN,"stat","| total executor add events    = %d", instance->stat.totalAddEvent);
                DEBUGP (DWARN,"stat","| total executor re-use events = %d", instance->stat.totalReuseEvent);
                DEBUGP (DWARN,"stat","| total executor exit events   = %d", instance->stat.numExecutorExit);
                DEBUGP (DWARN,"stat","| total exec token processed   = %d", instance->stat.numProcessed);
                DEBUGP (DWARN,"stat","| total feeders processed      = %d", instance->stat.numFeeder);
                DEBUGP (DWARN,"stat","| total linkers processed      = %d", instance->stat.numLinker);
                DEBUGP (DWARN,"stat","| total dropped tokens         = %d", instance->stat.numDropped);
                DEBUGP (DWARN,"stat","| num optimize calls           = %d", numOptimize);
                DEBUGP (DWARN,"stat","---------------------------------------------");

                memset (&instance->stat,0,sizeof (instance->stat));
                instance->lastStat = tsec;
            }
        }

        if (instance->execTimeout) {
            long int tsec = __tsec ();
            if (instance->stat.numProcessed == 0) {
                if ( tsec - instance->lastExec >= (instance->execTimeout / 1000) ) {

                    DEBUGP (DERR,"execTimeout","FATAL FAILURE: %ld seconds elapsed since last execution processing!",tsec - instance->lastExec);
                    SystemExit = TRUE;
                    return FALSE;
                    
                }
            } else {
                instance->lastExec = tsec;
            }
        }
    }

    numOptimize = 0;
    
    // we run monitor check every second
    sleep (1);
    return TRUE;
}

static inline void _destroyExecutor (void *data) {
    quark_t *executor = (quark_t *)data;
    if (executor) {
        I (Quark)->destroy (&executor);
    }
}


/*//////// API ROUTINES //////////////////////////////////////////*/

static transformation_processor_t *
newTransformationProcessor (transformation_matrix_t *matrix, parameters_t *params) {
    transformation_processor_t *instance = (transformation_processor_t *)calloc (1, sizeof (transformation_processor_t));
    if (instance) {
        int feeder_queue_size = I (Parameters)->getByteValue (params,"feeder_queue_size");
        int exec_queue_size   = I (Parameters)->getByteValue (params,"exec_queue_size");
        int feeder_queue_timeout = I (Parameters)->getTimeValue (params,"feeder_queue_timeout"); /* these are in us (yes, it is against the def */
        int exec_queue_timeout   = I (Parameters)->getTimeValue (params,"exec_queue_timeout");
        int prefork           = I (Parameters)->getByteValue (params,"prefork");
        int min_executors     = I (Parameters)->getByteValue (params,"min_threads");
        int max_executors     = I (Parameters)->getByteValue (params,"max_threads");
        int max_load_cond     = I (Parameters)->getByteValue (params,"max_load");
        int load_delay        = I (Parameters)->getTimeValue (params,"load_delay");
        int stat_interval     = I (Parameters)->getTimeValue (params,"stat_interval");
        int exec_timeout      = I (Parameters)->getTimeValue (params,"exec_timeout");
        boolean_t use_profiler = I (Parameters)->getBooleanValue (params,"use_profiler");
        boolean_t use_atomic = I (Parameters)->getBooleanValue (params,"use_atomic");
        boolean_t use_cache = I (Parameters)->getBooleanValue (params,"use_cache");
        
        if (feeder_queue_size < 0)
            feeder_queue_size = DEFAULT_FEEDER_QUEUE_SIZE;

        if (exec_queue_size < 0)
            exec_queue_size = DEFAULT_EXEC_QUEUE_SIZE;

        if (feeder_queue_timeout < 0)
            feeder_queue_timeout = DEFAULT_FEEDER_QUEUE_TIMEOUT;

        /* 
         * if (exec_queue_timeout < 0)
         *     exec_queue_timeout = DEFAULT_EXEC_QUEUE_TIMEOUT;
         */

        if (prefork < 0)
            prefork = 0;
        
        if (min_executors <= 0)
            min_executors = 1;
        
        if (max_executors > DEFAULT_TRANSFORMATION_PROCESSOR_MAX_EXECUTORS)
            max_executors = DEFAULT_TRANSFORMATION_PROCESSOR_MAX_EXECUTORS;
        else if (max_executors < 1)
            max_executors = 1;

        if (max_load_cond < 0)
            max_load_cond = 0;

        if (load_delay < 0)
            load_delay = DEFAULT_TRANSFORMATION_LOAD_DELAY;

        if (stat_interval < 0)
            stat_interval = 0;

        if (exec_timeout < 0)
            exec_timeout = 0;
        
        DEBUGP (DINFO,"newTransformationProcessor","creating a new instance of TransformationProcessor...");
        DEBUGP (DINFO,"newTransformationProcessor","| feeder_queue_size = %d", feeder_queue_size);
        DEBUGP (DINFO,"newTransformationProcessor","| feeder_queue_timeout = %d", feeder_queue_timeout);
        DEBUGP (DINFO,"newTransformationProcessor","| exec_queue_size = %d", exec_queue_size);
        DEBUGP (DINFO,"newTransformationProcessor","| exec_queue_timeout = %d", exec_queue_timeout);
        DEBUGP (DINFO,"newTransformationProcessor","| prefork = %d", prefork);
        DEBUGP (DINFO,"newTransformationProcessor","| min_threads = %d", min_executors);
        DEBUGP (DINFO,"newTransformationProcessor","| max_threads = %d", max_executors);
        DEBUGP (DINFO,"newTransformationProcessor","| max_load = %d", max_load_cond);
        DEBUGP (DINFO,"newTransformationProcessor","| load_delay = %d", load_delay);
        DEBUGP (DINFO,"newTransformationProcessor","| stat_interval = %d", stat_interval);
        DEBUGP (DINFO,"newTransformationProcessor","| exec_timeout = %d", exec_timeout);
        DEBUGP (DINFO,"newTransformationProcessor","| use_profiler = %d", use_profiler);
        DEBUGP (DINFO,"newTransformationProcessor","| use_atomic = %d", use_atomic);
        DEBUGP (DINFO,"newTransformationProcessor","| use_cache = %d", use_cache);
        
        instance->executors = I (Array)->new ();
        /* make a BLOCKING feederQueue with a TIMEOUT */
        instance->feederQueue = I (TransformTokenQueue)->new (feeder_queue_size,feeder_queue_timeout);
        instance->execQueue = I (TransformTokenQueue)->new (exec_queue_size,exec_queue_timeout);
        instance->prefork = prefork;
        instance->minExecutors = min_executors;
        instance->maxExecutors = max_executors;
        instance->maxLoad = max_load_cond;
        instance->loadDelay = load_delay;
        instance->statInterval = stat_interval;
        instance->execTimeout = exec_timeout;
        instance->useAtomic = use_atomic;
        instance->useCache  = use_cache;
        
        MUTEX_SETUP (instance->lock);
        
        /* setup initial lastXXX timestamp values */
        instance->lastStat = __tsec ();
        instance->lastExec = __tsec ();

        if (use_profiler) {
            char *logfile = I (Parameters)->getValue (params,"profiler_log");
            if (!logfile) logfile = "/var/log/profiler.log";
            instance->profiler = I (File)->new (logfile,"a");
        }
        
        /*
         * combine common paths to get streamlined execution traversal
         */
        I (TransformationMatrix)->print (matrix);
        instance->matrix = matrix;
	fdctl(instance->fdbitmap, _INIT);

        instance->linkers.explicit = I (Cache)->new (linker_entry_cmp, linker_entry_del, 1000, 100000, NULL);
        instance->linkers.wild     = I (Cache)->new (linker_wild_entry_cmp, linker_entry_del, 500, 50000, NULL);
        
        if (!instance->executors || !instance->feederQueue || !instance->execQueue) {
            DEBUGP (DERR,"newTransformationProcessor","unable to create underlying executor array, feeder, exec queues!");
            I (Array)->destroy (&instance->executors,NULL);
            I (TransformTokenQueue)->destroy (&instance->feederQueue);
            I (TransformTokenQueue)->destroy (&instance->execQueue);
        }
    } else {
        DEBUGP (DERR,"newTransformationProcessor","cannot create instance of transformation data processor");
    }
	return instance;
}

static boolean_t
startTransformationProcessor (transformation_processor_t *instance) {
	if (instance) {
        DEBUGP (DINFO,"startTransformationProcessor","begin...");
        // if feeder queue is empty, initialize the feeder tokens
        if (!I (TransformTokenQueue)->count (instance->feederQueue)) {
            list_t *feederTargets = I (TransformationMatrix)->getFeederTargets (instance->matrix);
            list_item_t *item = NULL;
            instance->numFeederTokens = 0;
            while ((item = I (List)->pop (feederTargets))) {
                transform_target_t *target = (transform_target_t *)item->data;
                if (target) {
                    transform_object_t *feederObj = I (TransformObject)->new ("transform:feeder",target);
                    if (feederObj) {
                        transform_token_t *token = I (TransformToken)->new (target,feederObj);
                        if (token) {
                            if (!I (TransformTokenQueue)->put (instance->feederQueue, token)) {
                                DEBUGP (DERR,"startTransformationProcessor","cannot put a feeder token into feeder queue!");
                                // XXX - flush the feederQueue ???
                                return FALSE;
                            }
                            instance->numFeederTokens++;
                        }
                    }
                }
                I (ListItem)->destroy (&item);
            }
            I (List)->destroy (&feederTargets);
            if (instance->numFeederTokens) {
                DEBUGP (DINFO,"startTransformationProcessor","initialized %d feeder tokens!",instance->numFeederTokens);
            } else {
                DEBUGP (DERR,"startTransformationProcessor","no feeder tokens found in the Transformation Matrix! nothing to do!");
                return FALSE;
            }

            /* ALSO, initialize the linkers here */
            I (List)->destroy (&instance->linkers.all);
            instance->linkers.all = I (TransformationMatrix)->getLinkerTargets (instance->matrix);

            /* cache the linker results, explicit and wild */
            if (instance->linkers.all) {
                list_item_t *item = I (List)->first (instance->linkers.all);
                while (item) {
                    transform_target_t *target = (transform_target_t *)item->data;
                    transformation_t *xform = I (TransformationMatrix)->lookup (instance->matrix, target);
                    if (xform) {
                        cache_t *mycache = instance->linkers.explicit;
                        
                        if (strchr (xform->from,'*')) {
                            mycache = instance->linkers.wild;
                            DEBUGP (DDEBUG,"startTransformationProcessor","populating wildcard linker cache for '%s' @ %p",xform->from,mycache);
                        } else {
                            DEBUGP (DDEBUG,"startTransformationProcessor","populating explicit linker cache for '%s' @ %p",xform->from,mycache);
                        }
                        
                        linker_entry_t *entry = (linker_entry_t *) I (Cache)->get (mycache, xform->from);
                        if (!entry) {
                            entry = (linker_entry_t *)calloc (1,sizeof (linker_entry_t));
                            if (entry) {
                                entry->key = xform->from;
                                entry->targets = I (Array)->new ();
                                DEBUGP (DDEBUG,"startTransformationProcessor","insert '%s' into %p",entry->key, mycache);
                                I (Cache)->put (mycache, entry->key, entry, sizeof (linker_entry_t) + sizeof (array_t));
                            }
                        }

                        if (entry && entry->targets) {
                            if (mycache == instance->linkers.wild && I (Array)->count (entry->targets) > 0) {
                                DEBUGP (DWARN,"startTransformationProcessor","conflicting linker wildcard cache entries between '%s' and '%s' @ %p",entry->key, xform->from, mycache);
                                /* HACK, use the shorter length as the new entry->key */
                                if (strlen (xform->from) < strlen (entry->key)) {
                                    linker_entry_t *new = (linker_entry_t *) calloc (1,sizeof (linker_entry_t));
                                    if (new) {
                                        new->key = xform->from;
                                        new->targets = I (Array)->clone (entry->targets);
                                        DEBUGP (DWARN,"startTransformationProcessor","using %s as the new key",new->key);
                                        I (Cache)->put (mycache, new->key, new, sizeof (linker_entry_t) + sizeof (array_t));
                                        entry = new;
                                    }
                                }
                            }
                            I (Array)->add (entry->targets,target);
                        }
                    }
                    item = I (List)->next (item);
                }
            }
        }

        // if EXECUTOR COUNT does not match MATRIX RULE COUNT, make necessary adjustments
        /* 
         * if (I (Array)->count (instance->executors) != I (TransformationMatrix)->countRules (instance->matrix)) {
         *     int i = I (TransformationMatrix)->countRules (instance->matrix) - I (Array)->count (instance->executors);
         *     DEBUGP (DINFO,"startTransformationProcessor","adjusting executors by %d",i);
         *     if (i > 0) {
         *         // if CURRENT EXECUTOR COUNT is less, add more EXECUTORS
         *         while (i-- > 0) {
         *             // spin off an execution quark and add into EXECUTORS
         *             quark_t *quark = I (Quark)->new (_xformExec, instance);
         *             if (quark) {
         *                 I (Quark)->setname (quark, "executor");
         *                 I (Array)->add (instance->executors,quark);
         *             } else {
         *                 DEBUGP (DERR,"startTransformationProcessor","cannot instantiate a executor quark!");
         *                 // XXX - clean up and flush executors?
         *                 return FALSE;
         *             }
         *         }
         *     } else {
         *         // if CURRENT EXECUTOR COUNT is more, get rid of some EXECUTORS
         *         while (i++ < 0) {
         *             I (Array)->remove (instance->executors,_destroyExecutor);
         *         }
         *     }
         * }
         */

        /*
         * spin off all the threads!
         */
        if (!instance->feeder) {
            quark_t *quark = I (Quark)->new (_xformFeeder, instance);
            if (quark) {
                I (Quark)->setname (quark,"feeder");
                instance->feeder = quark;
            }
            DEBUGP (DINFO,"startTransformationProcessor","spinning the feeder");
            // HACK, do not need feeder thread?
            I (Quark)->spin (instance->feeder);
            
            /*
             * Here handle any pre_forking requirements
             * 
             * EXPERIMENTAL
             */
            sleep (1);
            if (instance->prefork) {
                int i = instance->prefork;
                while (--i) {
                    pid_t pid = fork ();
                    if (pid < 0) exit (1); /* fork error */
                    if (pid == 0) break;
                    // let parent continue to make more children
                }
            }
        }

        if (!instance->monitor) {
            quark_t *quark = I (Quark)->new (_xformMonitor, instance);
            if (quark) {
                I (Quark)->setname (quark,"monitor");
                instance->monitor = quark;
            }
            DEBUGP (DINFO,"startTransformationProcessor","spinning the monitor");
            I (Quark)->spin (instance->monitor);
        }
        return TRUE;
	}
	return FALSE;
}

static boolean_t
stopTransformationProcessor (transformation_processor_t *instance) {
    if (instance) {
        int retry = 500;
        int i = 0;
        I (Quark)->stop (instance->feeder, 100);
        I (Quark)->stop (instance->monitor, 5000);

        /*
         * here we are waiting until the execQueue becomes empty (flushing all pending executions)!
         */
        while (--retry && I (TransformTokenQueue)->count (instance->execQueue) > 0) {
            usleep (1000);
        }

        if (retry == 0) {
            DEBUGP (DWARN,"stop","unable to flush all items from exec queue!");
        }

        DEBUGP (DINFO,"stop","starting executor cleanup for %d threads...",I (Array)->count (instance->executors));
        
        for (i = 0; I (Array)->get(instance->executors, i); i++) {
            DEBUGP (DDEBUG,"stop","stopping executor #%d",i);
            I (Quark)->stop ((quark_t *) I (Array)->get (instance->executors, i),50);
        }

        /*
         * here we actively destroy all feeder tokens since when the
         * processor starts again, it will re-populate the feeder
         * tokens appropriately
         */
        while (I (TransformTokenQueue)->count (instance->feederQueue)) {
            transform_token_t *token = I (TransformTokenQueue)->get (instance->feederQueue);
            if (token)
                I (TransformToken)->destroy (&token);
        }
        DEBUGP (DDEBUG,"stop","executor cleanup complete...");
        
        return TRUE;
    }
    return FALSE;
}

static void
destroyTransformationProcessor (transformation_processor_t **pPtr) {
	if (pPtr) {
		transformation_processor_t *instance = *pPtr;
		if (instance) {

            if (I (TransformTokenQueue)->count (instance->execQueue) > 0) {
                I (TransformationProcessor)->stop (instance);
            }
            
            I (Array)->destroy (&instance->executors, _destroyExecutor);
            I (Quark)->destroy (&instance->feeder);
            I (Quark)->destroy (&instance->monitor);

            I (TransformTokenQueue)->destroy (&instance->feederQueue);
            I (TransformTokenQueue)->destroy (&instance->execQueue);
            I (TransformationMatrix)->destroy (&instance->matrix);

            MUTEX_CLEANUP (instance->lock);
            free (instance);
            *pPtr = NULL;
		}
	}
}

static boolean_t
reloadTransformationProcessor (transformation_processor_t *instance, transformation_matrix_t *matrix) {
    if (instance && matrix) {
        boolean_t different = FALSE;
        // compare the current instance->matrix and the new matrix
        if (I (TransformationMatrix)->countSets (instance->matrix) != I (TransformationMatrix)->countSets (matrix) ||
            I (TransformationMatrix)->countRules (instance->matrix) != I (TransformationMatrix)->countRules (matrix)) {
            different = TRUE;
        }

        if (!different) {
            // further checking...
            
			I (TransformationMatrix)->destroy (&matrix);
			fdctl(instance->fdbitmap, _CLEAN); 
        }
        
        // if different, then stop the current processor instance and load the new matrix and re-start!
        if (different) {
            SystemExit = TRUE;
            fdctl(instance->fdbitmap, _RELOAD);
            I (TransformationProcessor)->stop (instance);

            I (TransformationMatrix)->destroy (&instance->matrix);
            instance->matrix = matrix;
       
            I (Array)->destroy (&instance->executors, _destroyExecutor);
            I (Quark)->destroy (&instance->feeder);
            I (Quark)->destroy (&instance->monitor);
            instance->executors = I (Array)->new ();
            I (TransformTokenQueue)->clearup(&instance->feederQueue);
            I (TransformTokenQueue)->clearup(&instance->execQueue);
            I (Cache)->destroy (&instance->linkers.explicit);
            I (Cache)->destroy (&instance->linkers.wild);
            instance->linkers.explicit = I (Cache)->new (linker_entry_cmp, linker_entry_del, 1000, 100000, NULL);
            instance->linkers.wild     = I (Cache)->new (linker_wild_entry_cmp, linker_entry_del, 500, 50000, NULL);
            instance->waitingExecutors = 0;
            SystemExit = FALSE;
            I (TransformationProcessor)->start (instance);
        }

        return TRUE;
    }
    return FALSE;
}

static void
optimizeTransformationProcessor (transformation_processor_t *instance) {
    numOptimize++;
    if (instance) {
        int pendingExec = I (TransformTokenQueue)->count (instance->execQueue);
        /*
         * if there is work to do, but not enough waiting executors, then we add an additional executor!
         */

        /*
         * XXX - do not throttle executor creation, if desired, add below condition
         * (instance->waitingExecutors <= instance->minExecutors)
         */
        if (pendingExec > instance->waitingExecutors) {
            // count active executors
            int activeExecutors = 0;
            int i = 0, dead = -1;
            quark_t *executor = NULL;
            for (i = 0; (executor = (quark_t *) I (Array)->get (instance->executors,i)); i++) {
                if (executor->stat & QUARK_STAT_RUNNING) {
                    activeExecutors++;
                } else {
                    dead = i;
                }
            }
            DEBUGP (DINFO,"optimizeTransformationProcessor","currently %d active executors, %d feederTokens, %d tokens inside ExecQueue",
                    activeExecutors, instance->numFeederTokens, pendingExec);

            if (activeExecutors > instance->stat.maxExecutors)
                instance->stat.maxExecutors = activeExecutors;

            if (pendingExec > instance->stat.maxPendingExec)
                instance->stat.maxPendingExec = pendingExec;
            
            /* now, check if we CAN in fact add more executors! */
            if (activeExecutors < instance->maxExecutors) {
                if (!instance->maxLoad || instance->currentLoad < instance->maxLoad || activeExecutors < instance->minExecutors) {

                    if (dead != -1) {
                        quark_t *reuse = (quark_t *) I (Array)->get (instance->executors,dead);
                        // reuse any older executors lying dead
                        I (Quark)->spin (reuse);
                        DEBUGP (DINFO,"optimizeTransformationProcessor","re-used an executor, now %d executors active!", activeExecutors+1);
                        instance->stat.totalReuseEvent++;
                    } else {
                        int j = instance->minExecutors - activeExecutors;

                        if (j <= 0) { /* already have min Executors, how much to grow by? */
                            j = ((pendingExec - instance->waitingExecutors) * (instance->maxExecutors - activeExecutors) + 1) /
                                ((pendingExec - instance->waitingExecutors) + (instance->maxExecutors - activeExecutors));
                            if (j > 0) j = 1; /* one at a time... */
                        }
                        
                        while (j-- > 0) {
                            // create a new executor instance!
                            quark_t *quark = I (Quark)->new (_xformExec, instance);
                            if (quark) {
                                I (Quark)->setname (quark, "executor");
                                I (Array)->add (instance->executors,quark);
                                I (Quark)->spin (quark);
                                DEBUGP (DINFO,"optimizeTransformationProcessor","added an executor, now %d executors active!", activeExecutors+1);
                                instance->stat.totalAddEvent++;
                            } else {
                                DEBUGP (DERR,"optimizeTransformationProcessor","cannot instantiate an additional executor quark!");
                                // XXX - clean up and flush executors?
                                break;
                            }
                        }
                    }

                    /*
                     * check if there is in fact a feeder token inside the exec queue
                     *
                     * if feeder stuck inside, we expedite the optimization
                     */
                    

                    
                } else {
                    DEBUGP (DWARN,"optimizeTransformationProcessor","cannot add more executors when load avg of %u (%f) exceeds %u (%f)",instance->currentLoad, (float)instance->currentLoad/100, instance->maxLoad, (float)instance->maxLoad/100);
                    usleep (instance->loadDelay * 1000);
                }
            } else {
                DEBUGP (DWARN,"optimizeTransformationProcessor","cannot add more executors beyond max of %d",instance->maxExecutors);
                usleep (instance->loadDelay * 1000);
            }
        }
    }
}

IMPLEMENT_INTERFACE (TransformationProcessor) = {
	.new       = newTransformationProcessor,
	.destroy   = destroyTransformationProcessor,
	.start     = startTransformationProcessor,
	.stop      = stopTransformationProcessor,
    .reload    = reloadTransformationProcessor,
    .optimize  = optimizeTransformationProcessor,
    
    /* below are optional, not implemented here */
    .pause     = NULL,
    .resume    = NULL
};

/***** DataProcessor Interface Implementation *****/

static transformation_matrix_t *
_getTransformationMatrixFromConfiguration (configuration_t *conf) {
    transformation_matrix_t *matrix = NULL;
    configuration_t *xConfig = NULL;
    category_t *dataProcessor = I (Configuration)->getCategory (conf,"DataProcessor");
    if (dataProcessor && I (Category)->getParamValue (dataProcessor,"transformation")) {
        xConfig = I (XformConfigParser)->parse (I (Category)->getParamValue (dataProcessor,"transformation"));
    } else {
        xConfig = I (Configuration)->copy (conf);
    }

    if (xConfig) {
        transform_engine_t *engine = I (TransformEngine)->new (xConfig);
        if (engine) {
            matrix = I (TransformEngine)->resolve (I (TransformEngine)->new (xConfig));
        } else {
            DEBUGP (DERR,"_getTransformationMatrixFromConfiguration","cannot create instance of transform engine!");
        }
        I (TransformEngine)->destroy (&engine);
        I (Configuration)->destroy (&xConfig);
    }
    return matrix;
}

static data_processor_t *
newDataProcessor (configuration_t *conf) {
	data_processor_t *proc = (data_processor_t *)calloc (1,sizeof (data_processor_t));
	if (proc) {
        transformation_matrix_t *matrix = _getTransformationMatrixFromConfiguration (conf);
        if (matrix) {
            category_t *cat = I (Configuration)->getCategory (conf,"DataProcessor");
            proc->instance = I (TransformationProcessor)->new (matrix, cat?I (Category)->getParameters (cat):NULL);
            if (!proc->instance) {
                DEBUGP (DERR,"newDataProcessor","cannot create instance of transformation data processor!");
                I (DataProcessor)->destroy (&proc);
            }
        } else {
            DEBUGP (DERR,"newDataProcessor","cannot create instance of data processor without valid configuration!");
            I (DataProcessor)->destroy (&proc);
        }
    }
    return proc;
}

static void
destroyDataProcessor (data_processor_t **pPtr) {
	if (pPtr) {
		data_processor_t *processor = *pPtr;
		if (processor) {
            transformation_processor_t *instance = (transformation_processor_t *) processor->instance;
            if (instance) {
                I (TransformationProcessor)->destroy (&instance);
            }
            free (processor);
            *pPtr = NULL;
        }
    }
}

static boolean_t
startDataProcessor (data_processor_t *proc) {
	if (proc) {
        transformation_processor_t *instance = (transformation_processor_t *) proc->instance;
        if (instance) {
            return I (TransformationProcessor)->start (instance);
        }
    }
    return FALSE;
}

static boolean_t
stopDataProcessor (data_processor_t *proc) {
	if (proc) {
        transformation_processor_t *instance = (transformation_processor_t *) proc->instance;
        if (instance) {
            return I (TransformationProcessor)->stop (instance);
        }
    }
    return FALSE;
}

static boolean_t
reloadDataProcessor (data_processor_t *proc, configuration_t *conf) {
    if (proc && conf) {
        transformation_processor_t *instance = (transformation_processor_t *) proc->instance;
        if (instance) {
            return I (TransformationProcessor)->reload (instance, _getTransformationMatrixFromConfiguration (conf));
        }
    }
    return FALSE;
}

IMPLEMENT_INTERFACE (DataProcessor) = {
	.new       = newDataProcessor,
	.destroy   = destroyDataProcessor,
	.start     = startDataProcessor,
	.stop      = stopDataProcessor,
    .reload    = reloadDataProcessor,
    .pause     = NULL,
    .resume    = NULL
};
