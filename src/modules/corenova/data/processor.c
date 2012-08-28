#include <corenova/source-stub.h>

THIS = {
	.version     = "2.1",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "This module provides API interface for data processing",
	.implements  = LIST ("DataProcessor"),
	.requires    = LIST ("corenova.data.configuration",
                         "corenova.sys.quark",
						 "corenova.sys.loader")
};

#include <corenova/data/processor.h>

#include <corenova/data/configuration.h>
#include <corenova/sys/loader.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

/***** DataProcessor Interface Implementation *****/

static data_processor_t *
_newDataProcessor (configuration_t *conf) {
    if (conf) {
        category_t *processorConf = I (Configuration)->getCategory (conf,"DataProcessor");
        module_t *module =
			I (DynamicLoader)->load (I (Category)->getParamValue (processorConf,"module"));
		if (module) {
			if (I_ACCESS (module, DataProcessor)) {
				data_processor_t *proc =
					I_ACCESS (module, DataProcessor)->new (conf);
				if (proc) {
//                    proc->conf = I (Configuration)->copy (conf); /* make a copy */
					proc->module = module;
					return proc;
				}
			}
		}
	}
	return NULL;
}

static void
_destroyDataProcessor (data_processor_t **pPtr) {
	if (pPtr && *pPtr) {
        data_processor_t *proc = *pPtr;
        if (proc) {
            module_t *module = proc->module;
            if (module) {
                I_ACCESS (module, DataProcessor)->destroy (pPtr);
                I (DynamicLoader)->unload (module);
            }
            if (*pPtr) {
                free (*pPtr);
                *pPtr = NULL;
            }
        }
	}
}

static boolean_t
_startDataProcessor (data_processor_t *proc) {
	if (proc) {
		module_t *module = proc->module;
		if (module) {
			return I_ACCESS (module, DataProcessor)->start (proc);
		}
	}
	return FALSE;
}

static boolean_t
_stopDataProcessor (data_processor_t *proc) {
	if (proc) {
		module_t *module = proc->module;
		if (module) {
			return I_ACCESS (module, DataProcessor)->stop (proc);
		}
	}
	return FALSE;
}

static boolean_t
_reloadDataProcessor (data_processor_t *proc, configuration_t *conf) {
	if (proc) {
		module_t *module = proc->module;
		if (module) {
            return I_ACCESS (module, DataProcessor)->reload (proc,conf);
		}
	}
    return FALSE;
}

static void
_pauseDataProcessor (data_processor_t *proc) {
	if (proc) {
		module_t *module = proc->module;
		if (module && I_ACCESS (module, DataProcessor)->pause) {
			return I_ACCESS (module, DataProcessor)->pause (proc);
		}
	}
}

static void
_resumeDataProcessor (data_processor_t *proc) {
	if (proc) {
		module_t *module = proc->module;
		if (module && I_ACCESS (module, DataProcessor)->resume) {
			return I_ACCESS (module, DataProcessor)->resume (proc);
		}
	}
}

IMPLEMENT_INTERFACE (DataProcessor) = {
	.new       = _newDataProcessor,
	.destroy   = _destroyDataProcessor,
	.start     = _startDataProcessor,
	.stop      = _stopDataProcessor,
    .reload    = _reloadDataProcessor,
    .pause     = _pauseDataProcessor,
    .resume    = _resumeDataProcessor
};

