#ifndef __processor_transformation_H__
#define __processor_transformation_H__

#include <corenova/interface.h>

#include <corenova/data/array.h>
#include <corenova/data/cache.h>
#include <corenova/sys/quark.h>
#include <corenova/sys/transform.h>

#include <corenova/data/file.h>
typedef file_t transformation_profiler_t;

#define FD_MAP_MAX 128
typedef struct {

	quark_t *feeder;            /* holds feeder thread */
    array_t *executors;         /* holds execution threads */
    quark_t *monitor;           /* holds monitor thread */

    uint32_t prefork;          /* holds number of prefork instances to create */
    uint32_t minExecutors;     /* holds minimum number of executors that should be running at all times */
    uint32_t maxExecutors;     /* holds maximum number of executors allowed */
    uint32_t maxLoad;          /* holds maximum load condition (0 for unlimited) x100 standard load float */
    uint32_t loadDelay;        /* holds load delay between exec attempt (in ms) */
    uint32_t statInterval;     /* holds delay between stat print (in secs) */
    uint32_t execTimeout;      /* holds execution processing timeout (in secs) */

    boolean_t useAtomic; /* atomic execution, set true on multi core systems */
    boolean_t useCache;  /* enable linker lookup caching, can help slower systems */
    
    transform_token_queue_t *feederQueue;
    transform_token_queue_t *execQueue;

    transformation_matrix_t *matrix; /* holds transformation engine resolved matrix */

    struct {
        list_t *all;            /* holds current linkage information */
        cache_t *explicit;
        cache_t *wild;
    } linkers;

    int numFeederTokens;

    MUTEX_TYPE lock;
    int currentLoad;
    int waitingExecutors;
    
    long int lastExec;
    long int lastStat;
    struct {
        uint32_t maxExecutors;
        uint32_t maxPendingExec;
        uint32_t totalAddEvent;
        uint32_t totalReuseEvent;
        uint32_t numExecutorExit;
        uint32_t numProcessed;
        uint32_t numFeeder;
        uint32_t numLinker;
        uint32_t numDropped;
    } stat;

    transformation_profiler_t *profiler;
    
    uint32_t fdbitmap[ FD_MAP_MAX / 32 ]; /* fds */
} transformation_processor_t;

#define DEFAULT_TRANSFORMATION_PROCESSOR_MAX_EXECUTORS 8192
#define DEFAULT_FEEDER_QUEUE_SIZE 255
#define DEFAULT_EXEC_QUEUE_SIZE 255 * 10
#define DEFAULT_FEEDER_QUEUE_TIMEOUT 1 /* 1ms */
#define DEFAULT_EXEC_QUEUE_TIMEOUT 100 /* 100ms */
#define DEFAULT_TRANSFORMATION_LOAD_DELAY 10 /* 10ms */

/* Transform counter controller object for transform:counter xform->instance blueprint and other params */
typedef struct {
	char *format;
	uint32_t count; /* Count of the stat event - violations, transactions etc */
	struct timeval start_time;
	unsigned long interval; /* Timeout for summary - blueprint */
} transform_counter_controller_t;

/* transform counter object for sending reports to logger service*/
typedef struct {
	char *format; /* example - WEB.CONTENTFILTER.COMMTOUCH.TRANSACTIONS */
	uint32_t count; /* Count of the stat event - violations, transactions etc */
	unsigned long start; /* tv.sec + tv.usec - time when the transform counter object got created */
	unsigned long duration; /* Dynamic Logging summary timeout updated to logger service */
} transform_counter_t;

/*
 * Extended interface beyond basic DataProcessor with additional calls
 *
 * Normally, would not need to access directly, but provided if used
 * as an object outside of configuration-driven context
 */
#include <corenova/data/processor.h>

DEFINE_INTERFACE (TransformationProcessor) {

	transformation_processor_t *(*new)      (transformation_matrix_t *, parameters_t *);
	void                        (*destroy)  (transformation_processor_t **);
	boolean_t                   (*start)    (transformation_processor_t *);
	boolean_t                   (*stop)     (transformation_processor_t *);
    boolean_t                   (*reload)   (transformation_processor_t *, transformation_matrix_t *);
    void                        (*optimize) (transformation_processor_t *);
    void                        (*pause)    (transformation_processor_t *);
    void                        (*resume)   (transformation_processor_t *);
    
};

#endif
