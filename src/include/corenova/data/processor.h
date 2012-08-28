#ifndef __processor_H__
#define __processor_H__

#include <corenova/interface.h>

#include <corenova/sys/loader.h>
#include <corenova/data/configuration.h>

typedef struct {

	module_t     *module;
	void         *instance;     /* holds instance state */
	
} data_processor_t;

DEFINE_INTERFACE (DataProcessor) {

	data_processor_t *(*new)       (configuration_t *);
	void              (*destroy)   (data_processor_t **);
	boolean_t         (*start)     (data_processor_t *);
	boolean_t         (*stop)      (data_processor_t *);
    boolean_t         (*reload)    (data_processor_t *, configuration_t *);
    void              (*pause)     (data_processor_t *); /* base Quark pause, optional extension */
    void              (*resume)    (data_processor_t *); /* base Quark resume, optional extension */
};

#endif
