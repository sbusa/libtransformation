#ifndef __transform_H__
#define __transform_H__

#include <corenova/interface.h>

/*----------------------------------------------------------------*/

typedef void (*XDESTROY) (void **);

typedef struct _transform_obj {

    char ALIGNED64 *format;     /* data format */
    void ALIGNED64 *data;       /* actual data */

    /*
     * following for use by TransformEngine
     */
    boolean_t save;             /* mark the data to be preserved! */
    uint32_t  access;           /* counter for number of access */
    module_t *destructor;       /* module that should destroy this object */
    XDESTROY  destroy;

    MUTEX_TYPE lock;            /* object-specific lock */
    
    struct _transform_obj *originator; /* encapsulation */
    
} transform_object_t;

DEFINE_INTERFACE (TransformObject) {
    
	transform_object_t *(*new)     (const char *format, void *data);
    transform_object_t *(*find)    (transform_object_t *, const char *format);
    transform_object_t *(*pop)     (transform_object_t *, const char *format);
    transform_object_t *(*split)   (transform_object_t *, uint16_t splitBy);
    void                (*attach)  (transform_object_t *, transform_object_t *orig);
    void                (*save)    (transform_object_t *);
	void                (*destroy) (transform_object_t **);
    
};

typedef struct {

    uint16_t set;
    uint16_t rule;
    uint16_t chain;

} transform_target_t;

typedef struct {

    transform_target_t *target;
    transform_object_t *obj;
    
} transform_token_t;

DEFINE_INTERFACE (TransformToken) {

    transform_token_t *(*new)      (transform_target_t *target, transform_object_t *obj);
	void               (*destroy)  (transform_token_t **);

};

/*----------------------------------------------------------------*/

#define TRANSFORM_EXEC(FUNC)                                            \
    static transform_object_t * FUNC (transformation_t * xform, transform_object_t *in)

#define TRANSFORM(FROM,TO,FUNC)                                         \
    if (I (String)->equalWild (from,FROM) && I (String)->equalWild (to,TO)) xform->exec = FUNC

#define TRANSFORM_WITH(INSTANCE)                                        \
    xform->instance = INSTANCE;                                         \
    if (!xform->instance) {                                             \
        DEBUGP (DERR,"Transformation",                         \
                "unable to initialize the instance for (%s -> %s)", from, to); \
        free (xform);                                                   \
        return NULL;                                                    \
    }

#define TRANSFORM_HAS_PARAM(PARAM)                                      \
    if (!I (Parameters)->getValue (blueprint,PARAM)) {                  \
        DEBUGP (DERR,"Transformation","transformation requires '%s' parameter defined!",PARAM); \
        free (xform);                                                   \
        return NULL;                                                    \
    }

#define IF_TRANSFORM(FUNC) if (xform->exec == FUNC)

#define TRANSFORM_NEW(FUNC)                                             \
    static transformation_t *                                           \
    FUNC (const char *from, const char *to, parameters_t *blueprint) {  \
        if (from && to && blueprint) {                                  \
            transformation_t *xform = (transformation_t *)calloc (1,sizeof (transformation_t)); \
            if (xform) {

#define TRANSFORM_NEW_FINALIZE                                          \
    if (!xform->exec) {                                                 \
        DEBUGP (DERR,"Transformation","transformation %s -> %s is not supported!", from, to); \
        free (xform);                                                   \
        return NULL;                                                    \
    }                                                                   \
    xform->module = SELF;                                               \
    xform->from = strdup (from);                                        \
    xform->to   = strdup (to);                                          \
    xform->blueprint = I (Parameters)->copy(blueprint); return xform; }} return NULL; }

#define TRANSFORM_DESTROY(FUNC)                 \
    static void FUNC (transformation_t **ptr) { \
        if (ptr) {                              \
            transformation_t *xform = *ptr;     \
            if (xform) {

#define TRANSFORM_DESTROY_FINALIZE                                      \
    free (xform->from); free (xform->to);                               \
    I (Parameters)->destroy (&xform->blueprint); free (xform); *ptr = NULL; }}}

#define TRANSFORM_FREE(FUNC)                    \
    static void FUNC (transform_object_t *obj)  \

#include <corenova/data/parameters.h>

typedef struct __transformation {

    module_t     *module;
    char         *from;
    char         *to;
    parameters_t *blueprint;
    void         *instance;
    int           type;
    transform_object_t *(*exec) (struct __transformation *, transform_object_t *in);
    
} transformation_t;

DEFINE_INTERFACE (Transformation) {
    
    transformation_t   *(*new)     (const char *from, const char *to, parameters_t *blueprint);
    transform_object_t *(*execute) (transformation_t *, transform_object_t *in);
    void                (*free)    (transform_object_t *obj);
    void                (*destroy) (transformation_t **);
    
};

/*----------------------------------------------------------------*/

#include <corenova/data/list.h>
#include <corenova/data/array.h>

typedef struct {

    array_t *targets;           /* all TransformTargets present in sets */
    array_t *sets;
    
} transformation_matrix_t;

DEFINE_INTERFACE (TransformationMatrix) {

    transformation_matrix_t *(*new)              (array_t *sets);
    list_t                  *(*getFeederTargets) (transformation_matrix_t *);
    list_t                  *(*getLinkerTargets) (transformation_matrix_t *);
    list_t                  *(*getNextTargets)   (transformation_matrix_t *, transform_target_t *);
    transformation_t        *(*lookup)           (transformation_matrix_t *, transform_target_t *);
    int                      (*countSets)        (transformation_matrix_t *);
    int                      (*countRules)       (transformation_matrix_t *);
    void                     (*print)            (transformation_matrix_t *);
    void                     (*destroy)          (transformation_matrix_t **);
    
};

/*----------------------------------------------------------------*/

#include <corenova/data/configuration.h>
#include <corenova/data/array.h>

typedef struct {

    array_t *rules;             /* list of transformation rules */
    array_t *chaos;             /* list of all available transformations (supported by modules) */
    
} transform_engine_t;

DEFINE_INTERFACE (TransformEngine) {

    transform_engine_t      *(*new)        (configuration_t *);
    boolean_t                (*addRule)    (transform_engine_t *, const char *rule, parameters_t *blueprint);
    void                     (*printRules) (transform_engine_t *);
    transformation_matrix_t *(*resolve)    (transform_engine_t *);
    void                     (*destroy)    (transform_engine_t **);
    
};

/*----------------------------------------------------------------*/

#include <corenova/data/queue.h>

typedef cqueue_t transform_object_queue_t;

DEFINE_INTERFACE (TransformObjectQueue) {
	transform_object_queue_t *(*new)     (uint32_t maxSize);
	transform_object_t       *(*get)     (transform_object_queue_t *);
	boolean_t                 (*put)     (transform_object_queue_t *, transform_object_t *);
	void                      (*destroy) (transform_object_queue_t **);
    
};

typedef cqueue_t transform_token_queue_t;

DEFINE_INTERFACE (TransformTokenQueue) {
	transform_token_queue_t *(*new)     (uint32_t maxSize, int timeout_ms);
	transform_token_t       *(*get)     (transform_token_queue_t *);
	boolean_t                (*put)     (transform_token_queue_t *, transform_token_t *);
    uint32_t                 (*count)   (transform_token_queue_t *);
	void                     (*destroy) (transform_token_queue_t **);
    
};

#endif
