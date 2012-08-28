#include <corenova/source-stub.h>

THIS = {
	.version     = "2.1",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "This module acts as a generic data processor dealing with inputs & outputs",
	.implements  = LIST ("DataProcessor","GenericDataProcessor","DataSource","DataOutput"),
	.requires    = LIST ("corenova.data.configuration",
                         "corenova.data.list",
						 "corenova.data.string",
                         "corenova.data.md5",
						 "corenova.sys.loader",
						 "corenova.sys.quark")
};

#include <corenova/data/processor/generic.h>
#include <corenova/data/string.h>
#include <corenova/data/md5.h>
#include <corenova/sys/loader.h>
#include <corenova/sys/quark.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <unistd.h>             /* for usleep */

/*//////// HELPER ROUTINES //////////////////////////////////////////*/

/*
 * Heart of the GENERIC processor.
 * 
 * Takes stuff from SOURCE(s) and send it off to OUTPUT(s).
 */ 
static boolean_t
_genericProcessorLoop (void *inData) {
	generic_processor_t *instance = (generic_processor_t *)inData;
    if (instance) {
        unsigned long
            numSources = 0,
            numOutputs = 0,
            numProcessed = 0,
            numDropped   = 0,
            numDataItems = 0;
			
        /* get from input and put into output,
         * but first check whether ANY output is ready! */
        list_item_t *outCheckItem = I (List)->first (instance->outputs);
        while (outCheckItem) {
            data_output_t *outCheck = (data_output_t *)outCheckItem->data;
            if (outCheck) {
                if (I (DataOutput)->check (outCheck)) {
                    numOutputs++;
                }
            }
            outCheckItem = I (List)->next (outCheckItem);
        }
		
        if (numOutputs) {
            list_item_t *srcItem = I (List)->first (instance->sources);
            while (srcItem) {
                data_source_t *source = (data_source_t *)srcItem->data;
                if (source) {
                    uint32_t dataCounter = 0;
                    data_item_t *data = NULL;
                    numSources++;
                    while ((data = I (DataSource)->get (source))) {
                        list_item_t *outItem = I (List)->first (instance->outputs);
                        dataCounter++;
                        numDataItems++;
                        while (outItem) {
                            data_output_t *output = (data_output_t *)outItem->data;
                            if (output) {
                              put_again:
                                if (I (DataOutput)->put (output, data)) {
                                    numProcessed++;
                                } else {
                                    usleep (5000); /* we don't like dropping stuff! */
                                    goto put_again;
                                    /* numDropped++; */
                                }
                            }
		
                            outItem = I (List)->next (outItem);
                        }
		
                        if (dataCounter >= instance->threshold) {
                            break;
                        }
                    }
					
                    if (dataCounter) {
                        DEBUGP (DINFO,"genericProcessorLoop","processed %lu data items from (%s)",
                                (unsigned long)dataCounter,source->module->name);
                    }
                }
                srcItem = I (List)->next (srcItem);
            }

            if (numDataItems) {
                DEBUGP (DINFO,"genericProcessorLoop","processed %lu data items from %lu sources, put %lu items into %lu outputs, dropped %lu items.",
                        numDataItems, numSources, numProcessed, numOutputs, numDropped);
            } else {
                usleep (10000);		/* give it a breather... */
            }
        } else {
            usleep (15000);
        }
    }
	return TRUE;
}

/*//////// API ROUTINES //////////////////////////////////////////*/

static generic_processor_t *
newGenericProcessor (configuration_t *conf) {
    generic_processor_t *instance = (generic_processor_t *) calloc (1, sizeof (generic_processor_t));
    if (instance) {
        uint32_t threshold = DEFAULT_GENERIC_PROCESSOR_THRESHOLD;
        category_t *dataProcessor = I (Configuration)->getCategory (conf,"DataProcessor");
        if (dataProcessor && I (Category)->getParamValue (dataProcessor,"threshold")) {
            threshold = atoi (I (Category)->getParamValue (dataProcessor,"threshold"));
        }
        I (GenericDataProcessor)->setThreshold (instance,threshold);
            
        instance->sources = I (List)->new ();
        if (instance->sources) {
            category_t *dataSource = I (Configuration)->getCategory (conf,"DataSource");
            while (dataSource) {
                data_source_t *source = I (DataSource)->new (I (Category)->getParameters (dataSource));
                if (source) {
                    if (I (GenericDataProcessor)->addSource (instance,source)) {
                        DEBUGP (DDEBUG,"newDataProcessor","added a new data source into processor.");
                    } else {
                        I (DataSource)->destroy (&source);
                    }
                } else {
                    DEBUGP (DERR,"newDataProcessor","ignoring invalid DataSource");
                }
                dataSource = I (Category)->next (dataSource);
            }
        }
        instance->outputs = I (List)->new ();
        if (instance->outputs) {
            category_t *dataOutput = I (Configuration)->getCategory (conf,"DataOutput");
            while (dataOutput) {
                data_output_t *output = I (DataOutput)->new (I (Category)->getParameters (dataOutput));
                if (I (GenericDataProcessor)->addOutput (instance,output)) {
                    DEBUGP (DDEBUG,"newDataProcessor","added a new data output into processor.");
                } else {
                    I (DataOutput)->destroy (&output);
                }
                dataOutput = I (Category)->next (dataOutput);
            }
        }
    }
    return instance;
}

static void
destroyGenericProcessor (generic_processor_t **pPtr) {
	if (pPtr) {
        generic_processor_t *instance = *pPtr;
        if (instance) {
            if (instance->quark) {
                I (Quark)->destroy (&instance->quark);
            }
            if (instance->sources) {
                I (List)->destroy (&instance->sources);
            }
            if (instance->outputs) {
                I (List)->destroy (&instance->outputs);
            }
            free (instance);
            *pPtr = NULL;
        }
	}
}

static boolean_t
startGenericProcessor (generic_processor_t *instance) {
    if (instance) {
        if (I (List)->count (instance->sources) &&
            I (List)->count (instance->outputs)) {
            quark_t *quark = I (Quark)->new (_genericProcessorLoop,instance);
            if (quark) {
                I (Quark)->setname(quark, "_genericProcessorLoop");
                instance->quark = quark;
                return I (Quark)->spin (quark);
            } else {
                DEBUGP (DERR,"startDataProcessor","cannot instantiate a quark!");
            }
        } else {
            DEBUGP (DERR,"startDataProcessor","must have DataSources AND DataOutputs defined!");
        }
    }
	return FALSE;
}
/*
 * checks list of modules and returns a list of modules that do not
 * match category configurations
 *
 * matching modules with a category causes the category object to be
 * flagged with __match__ = true
 */
static list_t *
__checkModules (list_t *modules, category_t *cats) {
    list_t *nomatch = I (List)->new ();
    if (modules && cats && nomatch) {
        category_t *category = cats;
        list_item_t *item = I (List)->first (modules);
        
        // iterate through current sources and compare with ones in conf...
        while (item) {
            data_module_t *module = (data_module_t *)item->data;
            if (module) {
                md5_t *checksum1 = I (Parameters)->md5 (module->params);

                /*
                 * look for a match in category configuration
                 */
                boolean_t match = FALSE;
                while (category) {
                    md5_t *checksum2 = I (Parameters)->md5 (I (Category)->getParameters (category));

                    if (I (MD5)->compare (checksum1,checksum2) == 0) { /* found a match! */
                        /* touch the configuration to mark the match */
                        I (Category)->setParameter (category,"__match__","true");
                        I (MD5)->destroy (&checksum2);
                        match = TRUE;
                        break;
                    }
                    I (MD5)->destroy (&checksum2);
                    category = I (Category)->next (category);
                }
                
                if (!match) {
                    // remove this item from passed in modules list and attach it to "nomatch" list for handling
                    I (List)->insert (nomatch, I (List)->remove (modules, item));
                }
                I (MD5)->destroy (&checksum1);
            }
            item = I (List)->next (item);
        }
    }
    return nomatch;
}

static boolean_t
reloadGenericProcessor (generic_processor_t *instance, configuration_t *conf) {
    if (instance && conf) {
        category_t *dataSource = I (Configuration)->getCategory (conf,"DataSource");
        category_t *dataOutput = I (Configuration)->getCategory (conf,"DataOutput");
        list_t *nomatchSources = __checkModules (instance->sources, dataSource);
        list_t *nomatchOutputs = __checkModules (instance->outputs, dataOutput);

        /*
         * first destroy all sources & outputs that do not have a match!
         */
        if (nomatchSources) {
            list_item_t *item = I (List)->first (nomatchSources);
            while (item) {
                data_source_t *source = (data_source_t *)item->data;
                I (DataSource)->destroy (&source);
                item = I (List)->next (item);
            }
            I (List)->destroy (&nomatchSources);
        }
        if (nomatchOutputs) {
            list_item_t *item = I (List)->first (nomatchOutputs);
            while (item) {
                data_output_t *output = (data_output_t *)item->data;
                I (DataOutput)->destroy (&output);
                item = I (List)->next (item);
            }
            I (List)->destroy (&nomatchOutputs);
        }

        /*
         * now add new sources and outputs from configuration if not marked with __match__!
         */
        I (GenericDataProcessor)->pause (instance);
        while (dataSource) {
            if (I (Category)->getParamValue (dataSource,"__match__") != NULL) {
                data_source_t *source = I (DataSource)->new (I (Category)->getParameters (dataSource));
                if (source) {
                    if (I (GenericDataProcessor)->addSource (instance,source)) {
                        DEBUGP (DDEBUG,"newDataProcessor","added a new data source into processor.");
                    } else {
                        I (DataSource)->destroy (&source);
                    }
                } else {
                    DEBUGP (DERR,"newDataProcessor","ignoring invalid DataSource");
                }
            }
            dataSource = I (Category)->next (dataSource);
        }
        while (dataOutput) {
            if (I (Category)->getParamValue (dataOutput,"__match__") != NULL) {
                data_output_t *output = I (DataOutput)->new (I (Category)->getParameters (dataOutput));
                if (output) {
                    if (I (GenericDataProcessor)->addOutput (instance,output)) {
                        DEBUGP (DDEBUG,"newDataProcessor","added a new data output into processor.");
                    } else {
                        I (DataOutput)->destroy (&output);
                    }
                } else {
                    DEBUGP (DERR,"newDataProcessor","ignoring invalid DataOutput");
                }
            }
            dataOutput = I (Category)->next (dataOutput);
        }
        I (GenericDataProcessor)->resume (instance);
            
        return TRUE;
    }
    return FALSE;
}

static void
pauseGenericProcessor (generic_processor_t *instance) {
    if (instance && instance->quark) {
        I (Quark)->pause (instance->quark);
    }
}

static void
resumeGenericProcessor (generic_processor_t *instance) {
    if (instance && instance->quark) {
        I (Quark)->unpause (instance->quark);
    }
}

static boolean_t
addGenericProcessorSource (generic_processor_t *instance, data_source_t *source) {
	if (instance && source) {
        if (I (DataSource)->activate (source)) {
            if (I (List)->insert (instance->sources,I (ListItem)->new (source))) {
                return TRUE;
            } else {
                DEBUGP (DERR,"addGenericProcessorSource","unable to add data source to processor sources list");
            }
        }
	}
	return FALSE;
}

static boolean_t
addGenericProcessorOutput (generic_processor_t *instance, data_output_t *output) {
	if (instance && output) {
        if (I (DataOutput)->activate (output)) {
            if (I (List)->insert (instance->outputs,I (ListItem)->new (output))) {
                return TRUE;
            } else {
                DEBUGP (DERR,"addGenericProcessorOutput","unable to add data output to processor outputs list");
            }
        }
	}
	return FALSE;
}

static void
setGenericProcessorThreshold (generic_processor_t *instance, uint32_t threshold) {
    if (instance) {
        instance->threshold = threshold;
        if (!instance->threshold) {
            instance->threshold = DEFAULT_GENERIC_PROCESSOR_THRESHOLD;
        }
    }
}

IMPLEMENT_INTERFACE (GenericDataProcessor) = {
	.new       = newGenericProcessor,
	.destroy   = destroyGenericProcessor,
	.start     = startGenericProcessor,
	.stop      = NULL,
    .reload    = reloadGenericProcessor,
    .pause     = pauseGenericProcessor,
    .resume    = resumeGenericProcessor,
	.addSource = addGenericProcessorSource,
	.addOutput = addGenericProcessorOutput,
    .setThreshold = setGenericProcessorThreshold
};

/***** DataProcessor Interface Implementation *****/

static data_processor_t *
newDataProcessor (configuration_t *conf) {
	data_processor_t *proc = (data_processor_t *)calloc (1,sizeof (data_processor_t));
	if (proc) {
        proc->instance = I (GenericDataProcessor)->new (conf);
        if (!proc->instance) {
            DEBUGP (DERR,"newDataProcessor","cannot create instance of generic data processor");
            free (proc);
            return NULL;
        }
    }
    return proc;
}

static void
destroyDataProcessor (data_processor_t **pPtr) {
	if (pPtr) {
		data_processor_t *processor = *pPtr;
		if (processor) {
            generic_processor_t *instance = (generic_processor_t *) processor->instance;
            if (instance) {
                I (GenericDataProcessor)->destroy (&instance);
            }
            free (processor);
            *pPtr = NULL;
        }
    }
}

static boolean_t
startDataProcessor (data_processor_t *proc) {
	if (proc) {
        generic_processor_t *instance = (generic_processor_t *) proc->instance;
        if (instance) {
            return I (GenericDataProcessor)->start (instance);
        }
    }
    return FALSE;
}

static boolean_t
reloadDataProcessor (data_processor_t *proc, configuration_t *conf) {
    if (proc && conf) {
        generic_processor_t *instance = (generic_processor_t *)proc->instance;
        if (instance) {
            return I (GenericDataProcessor)->reload (instance,conf);
        }
    }
    return FALSE;
}

static void
pauseDataProcessor (data_processor_t *proc) {
    if (proc) {
       generic_processor_t *instance = (generic_processor_t *)proc->instance;
        if (instance) {
            I (GenericDataProcessor)->pause (instance);
        }
    }
}

static void
resumeDataProcessor (data_processor_t *proc) {
    if (proc) {
       generic_processor_t *instance = (generic_processor_t *)proc->instance;
        if (instance) {
            I (GenericDataProcessor)->resume (instance);
        }
    }
}

IMPLEMENT_INTERFACE (DataProcessor) = {
	.new       = newDataProcessor,
	.destroy   = destroyDataProcessor,
	.start     = startDataProcessor,
	.stop      = NULL,
    .reload    = reloadDataProcessor,
    .pause     = pauseDataProcessor,
    .resume    = resumeDataProcessor
};

/***** DataSource Interface Implementation *****/

static data_source_t *
_newDataSource (parameters_t *params) {
	if (params) {
		module_t *module =
			I (DynamicLoader)->load (I (Parameters)->getValue (params,"module"));
		if (module) {
			if (I_ACCESS (module, DataSource)) {
				data_source_t *source =
					I_ACCESS (module, DataSource)->new (params);
				if (source) {
                    source->params = I (Parameters)->copy (params); /* make a copy */
					source->module = module;
					return source;
				}
			}
		}
	}
	return NULL;
}

static boolean_t
_activateDataSource (data_source_t *source) {
	if (source) {
		module_t *module = source->module;
		if (module) {
			return I_ACCESS (module, DataSource)->activate (source);
		}
	}
	return FALSE;
}

static data_item_t *
_getDataItem (data_source_t *source) {
	if (source) {
		module_t *module = source->module;
		if (module) {
			return I_ACCESS (module, DataSource)->get (source);
		}
	}
	return NULL;
}

static void
_destroyDataSource (data_source_t **source) {

	if(source && *source) {
        I (Parameters)->destroy (&(*source)->params);
	    module_t *module = (*source)->module;
	    if(module) {
            I_ACCESS(module, DataSource)->destroy(source);
            I (DynamicLoader)->unload (module);
        }
        if (*source) {
            free (*source);
            *source = NULL;
        }
	}
    
}

IMPLEMENT_INTERFACE (DataSource) = {
	.new      = _newDataSource,
	.activate = _activateDataSource,
	.get      = _getDataItem,
	.destroy  = _destroyDataSource
};

/***** DataOutput Interface Implementation *****/

static data_output_t *
_newDataOutput (parameters_t *params) {
	if (params) {
		module_t *module =
			I (DynamicLoader)->load (I (Parameters)->getValue (params,"module"));
		if (module) {
			if (I_ACCESS (module, DataOutput)) {
				data_output_t *output =
					I_ACCESS (module, DataOutput)->new (params);
				if (output) {
                    output->params = I (Parameters)->copy (params); /* make a copy */
					output->module = module;
					return output;
				}
			}
		}
	}
	return NULL;
}

static boolean_t
_activateDataOutput (data_output_t *output) {
	if (output) {
		module_t *module = output->module;
		if (module) {
			return I_ACCESS (module, DataOutput)->activate (output);
		}
	}
	return FALSE;
}

static boolean_t
_checkDataOutput (data_output_t *output) {
	if (output) {
		module_t *module = output->module;
		if (module) {
			return I_ACCESS (module, DataOutput)->check (output);
		}
	}
	return FALSE;
}

static boolean_t
_putDataItem (data_output_t *output, data_item_t *item) {
	if (output) {
		module_t *module = output->module;
		if (module) {
			return I_ACCESS (module, DataOutput)->put (output,item);
		}
	}
	return FALSE;
}

static void
_destroyDataOutput (data_output_t **output) {

	if(output && *output) {
        I (Parameters)->destroy (&(*output)->params);
	    module_t *module = (*output)->module;
	    if(module) {
            I_ACCESS(module, DataOutput)->destroy(output);
            I (DynamicLoader)->unload (module);
        }
        if (*output) {
            free (*output);
            *output = NULL;
        }
	}
    
}

IMPLEMENT_INTERFACE (DataOutput) = {
	.new      = _newDataOutput,
	.activate = _activateDataOutput,
	.check    = _checkDataOutput,
	.put      = _putDataItem,
    .destroy  = _destroyDataOutput
};

