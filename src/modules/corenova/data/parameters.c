#include <corenova/source-stub.h>

THIS = {
	.version = "2.0",
	.author  = "Peter K. Lee <saint@corenova.com>",
	.description = "This module provides provides key/val type parameter utility.",
	.implements = LIST ("Parameters"),
	.requires = LIST ("corenova.data.string", "corenova.data.md5")
};

#include <corenova/data/parameters.h>
#include <corenova/data/string.h>
#include <corenova/data/md5.h>

/*//////// INTERNAL CODE //////////////////////////////////////////*/

static void
_destroyParams(param_t *p) {
	if (!p) return;
	if (p->next) _destroyParams(p->next);
	free(p->key); free(p->val); free(p);
}

static int
_compareParamKeys (const void *one, const void *two) {
    param_t *p1 = (param_t *)one;
    param_t *p2 = (param_t *)two;
    /* the arguments to this function are pointers to param_t objects */
    return strcmp ( p1->key, p2->key );
}

static parameters_t *
_sortParams (parameters_t *params) {
    parameters_t *newp = I (Parameters)->new ();
    if (newp && params) {
//        DEBUGP (DINFO,"_sortParams","convert to array...");
        /* convert to array so we can use the qsort function */
        param_t *parray = calloc (params->count, sizeof (param_t));
        if (parray) {
            param_t *p = params->first;
            int i = 0;
//            fprintf (stderr,"params: %d entries\n",params->count);
            while (p != NULL) {
//                fprintf (stderr,"parray[%d] - %s = %s\n",i,p->key,p->val);
                memcpy (&parray[i++],p,sizeof (param_t));
                p = p->next;
            }
            
//            DEBUGP (DINFO,"_sortParams","run qsort...");
            qsort (parray, params->count, sizeof (param_t), _compareParamKeys);
            
            /* convert back to parameters_t linked list */
            for (i = 0; i < params->count; i++) {
                  I (Parameters)->add (newp, parray[i].key, parray[i].val);
            }
            free (parray);
        }
    }
    return newp;
}

/*//////// INTERFACE ROUTINES //////////////////////////////////////////*/

static parameters_t *
newParameters (void) {
	parameters_t *p = (parameters_t *)calloc(1,sizeof(parameters_t));
	return p;
}

static void
destroyParameters (parameters_t **params) {
	if (params && *params) {
		_destroyParams ((*params)->first);
		free (*params);
		*params = NULL;
	}
}

static char *
toString (parameters_t *params) {
	char *outString = NULL;
    param_t *p = NULL;
    if (params) {
//        DEBUGP (DINFO,"toString","params has %d count",params->count);
        parameters_t *sorted = _sortParams (params);
//        DEBUGP (DINFO,"toString","sorted ok? %d",(int) sorted);
        p = sorted->first;
        while (p != NULL) {
            I (String)->join(&outString,p->key);
            I (String)->join(&outString," = ");
            I (String)->join(&outString,p->val);
            I (String)->join(&outString,"\n");
            p = p->next;
        }
        destroyParameters (&sorted);
    }
	return outString;
}

static param_t *
getParameter (parameters_t *params, const char *key) {
	if (params) {
		param_t *p = params->first;
		size_t len = strlen(key);
		while (p != NULL) {
			size_t pLen = strlen(p->key);
			if ((len == pLen) && !strncasecmp(p->key,key,len))
				return p;
			p = p->next;
		}
	}
	return NULL;
}

static char *
getParamValue (parameters_t *params, const char *key) {
	param_t *match = getParameter(params,key);
	return match?match->val:NULL;
}

static boolean_t
addParameter (parameters_t *params, const char *key, const char *value) {
	if (params && !getParameter(params,key)) {
		param_t *p = (param_t *)calloc(1,sizeof(param_t));
		if (p) {
			p->key = strdup(key);
			p->val = strdup(value);
			
			if (params->first) {
				params->last->next = p;
				params->last = p;
			} else {
				params->first = params->last = p;
			}
			params->count++;
			return TRUE;
		}
	}
	return FALSE;
}

/* if key does not exist, add, else update with new value */
static boolean_t
updateParameter (parameters_t *params, const char *key, const char *value) {
	if (params) {
		param_t *match = getParameter(params,key);
		if (match) {
			if (match->val) free(match->val);
			match->val = strdup(value);
			return TRUE;
		} else {
			return addParameter(params,key,value);
		}
	}
	return FALSE;
}

static parameters_t *
copyParameters (parameters_t *params) {
    parameters_t *copy = I (Parameters)->new ();
    if (copy && params) {
        param_t *p = params->first;
		while (p != NULL) {
            I (Parameters)->add (copy, p->key, p->val);
			p = p->next;
		}
    }
    return copy;
}

static parameters_t *
joinParameters (parameters_t *one, parameters_t *two) {
    if (one && two) {
        param_t *p = two->first;
        while (p != NULL) {
            addParameter (one,p->key,p->val);
            p = p->next;
        }
        return one;
    }
    return NULL;
}

static md5_t *
md5Parameters (parameters_t *params) {
    if (params) {
        return I (String)->md5 (I (Parameters)->toString (params));
    }
    return NULL;
}

static int
getParamByteValue (parameters_t *params, const char *key) {
    int size = -1;
    char *string = getParamValue (params,key);
    if (string) {
        char metric = '\0';
        int nummatch = sscanf (string,"%d%c",&size,&metric);
        switch (nummatch) {
          case 2:
              switch (metric) {
                case 'k':
                case 'K': size *= 1024; break;
                case 'm':
                case 'M': size *= 1024 * 1024; break;
              }
              break;
        }
    }
    return size;
}

static int
getParamNumValue (parameters_t *params, const char *key) {
    int size = -1;
    char *string = getParamValue (params,key);
    if (string) {
        int nummatch = sscanf (string,"%d",&size);
        if (nummatch < 1)
            size = -1;
    }
    return size;
}

/*
 * RETURNS: time value in ms unit (default w/o metrics treats as ms)
 */ 
static int
getParamTimeValue (parameters_t *params, const char *key) {
    int size = -1;
    char *string = getParamValue (params,key);
    if (string) {
        char metric[4] = { '\0', '\0', '\0', '\0' };
        int nummatch = sscanf (string,"%d%c%c%c",&size,&metric[0],&metric[1],&metric[2]);
        if (nummatch >= 2) { // must have metric defined!
            if (!I (String)->equal ("ms",metric)) {
                if (I (String)->equal ("min",metric) || I (String)->equal ("m",metric)) {
                    size *= 1000 * 60;
                } else if (I (String)->equal ("sec",metric) || I (String)->equal ("s",metric)) {
                    size *= 1000;
                } else {
                    size = -1;
                }
            }
        } else if (nummatch < 1) {
            size = -1;
        }
    }

    if (size == -1) {
        DEBUGP (DDEBUG,"getParamTimeValue","error parsing parameter value (%s) into timeval!",key);
    }
    return size;
}

static boolean_t
getBooleanValue (parameters_t *params, const char *key) {
    char *string = getParamValue (params,key);
    if (string) {
        return I (String)->equal ("true",string) || I (String)->equal ("yes",string) || I (String)->equal ("on",string);
    }
    return FALSE;
}

IMPLEMENT_INTERFACE (Parameters) = {
	.new      = newParameters,
	.destroy  = destroyParameters,
	.add      = addParameter,
	.get      = getParameter,
	.getValue = getParamValue,
	.update   = updateParameter,
	.toString = toString,
    .copy     = copyParameters,
    .join     = joinParameters,
    .md5      = md5Parameters,
    .getByteValue = getParamByteValue,
    .getTimeValue = getParamTimeValue,
    .getBooleanValue = getBooleanValue,
    .getNumValue = getParamNumValue
};
