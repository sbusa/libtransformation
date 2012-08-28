#ifndef __processor_generic_H__
#define __processor_generic_H__

#include <corenova/interface.h>

#include <corenova/data/parameters.h>
#include <corenova/data/object.h>
typedef binary_t data_item_t;

typedef struct {

	module_t     *module;
    parameters_t *params; 
	void         *instance;     /* holds instance state */
    
} data_module_t;

typedef data_module_t data_source_t;

DEFINE_INTERFACE (DataSource) {
    
	data_source_t *(*new)      (parameters_t *);
	boolean_t      (*activate) (data_source_t *);
	data_item_t   *(*get)      (data_source_t *);
	void           (*destroy)  (data_source_t **);
    
};

typedef data_module_t data_output_t;

DEFINE_INTERFACE (DataOutput) {
    
	data_output_t *(*new)      (parameters_t *);
	boolean_t      (*activate) (data_output_t *);
	boolean_t      (*check)    (data_output_t *);
	boolean_t      (*put)      (data_output_t *, data_item_t *);
	void           (*destroy)  (data_output_t **);
	
};

#include <corenova/data/list.h>
#include <corenova/sys/quark.h>

#define DEFAULT_GENERIC_PROCESSOR_THRESHOLD 100

typedef struct {

	quark_t *quark;        /* holds processor runtime thread */
	list_t  *sources;
	list_t  *outputs;

	uint32_t threshold;
	
} generic_processor_t;

/*
 * Extended interface beyond basic DataProcessor with additional calls
 * (add source/output)
 *
 * Normally, would not need to access directly, but provided if used
 * as an object outside of configuration-driven context
 */
#include <corenova/data/processor.h>

DEFINE_INTERFACE (GenericDataProcessor) {

	generic_processor_t *(*new)          (configuration_t *);
	void                 (*destroy)      (generic_processor_t **);
	boolean_t            (*start)        (generic_processor_t *);
	boolean_t            (*stop)         (generic_processor_t *);
    boolean_t            (*reload)       (generic_processor_t *, configuration_t *);
    void                 (*pause)        (generic_processor_t *);
    void                 (*resume)       (generic_processor_t *);
    boolean_t            (*addSource)    (generic_processor_t *, data_source_t *);
	boolean_t            (*addOutput)    (generic_processor_t *, data_output_t *);
    void                 (*setThreshold) (generic_processor_t *, uint32_t threshold);
    
};

#endif
