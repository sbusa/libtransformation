#include <corenova/source-stub.h>

THIS = {
	.version     = "3.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "This is the uber cool transformation engine",
	.implements  = LIST ("TransformObject","TransformToken","TransformEngine","Transformation","TransformationMatrix","TransformObjectQueue","TransformTokenQueue"),
	.requires    = LIST ("corenova.data.configuration",
                         "corenova.data.array",
                         "corenova.data.list",
						 "corenova.data.string",
                         "corenova.data.md5",
                         "corenova.data.queue",
						 "corenova.sys.loader"),
    /*
     * BASIC set of transformations available:
     *
     *  transform:module -> transform:engine (provide ability to load modules into the engine)
     */ 
    .transforms = LIST ("transform:module -> transform:engine")
};

#include <corenova/sys/transform.h>
#include <corenova/data/configuration.h>
#include <corenova/data/array.h>
#include <corenova/data/list.h>
#include <corenova/data/md5.h>
#include <corenova/data/string.h>
#include <corenova/data/queue.h>
#include <corenova/sys/loader.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

/*//////// TransformObject Interface Implementation //////////////////////////////////////////*/

static transform_object_t *
newTransformObject (const char *format, void *data) {
    transform_object_t *obj = (transform_object_t *)calloc (1, sizeof (transform_object_t));
    if (obj) {
        obj->data = data;
        obj->format = (char *) format;
        obj->access = 1;
        obj->save = FALSE;

        MUTEX_SETUP (obj->lock);
        
        DEBUGP (DDEBUG,"newTransformObject","created a new object for '%s' with %p",format, data);
    }
    return obj;
}

static transform_object_t *
splitTransformObject (transform_object_t *obj, uint16_t splitBy) {
    if (obj && splitBy > 1) {
        //splitTransformObject (obj->originator, splitBy);
        MUTEX_LOCK (obj->lock);
        obj->access += splitBy-1;
        MUTEX_UNLOCK (obj->lock);
    }
    return obj;
}

static void
attachTransformObject (transform_object_t *obj, transform_object_t *orig) {
    if (obj && orig) {
        //DEBUGP (DDEBUG,"attachTransformObject","(%s)->originator = (%s)",obj->format,orig->format);
        obj->originator = orig;
    }
}

static transform_object_t *
findTransfromObject (transform_object_t *obj, const char *format) {
    if (obj && format) {
//        MUTEX_LOCK (obj->lock);
        do {
            if (I (String)->equalWild (obj->format,format)) {
//                MUTEX_UNLOCK (obj->lock);
                return obj;
            }
        } while ((obj = obj->originator));
//        MUTEX_UNLOCK (obj->lock);
    }
    return NULL;
}

static transform_object_t *
popTransformObject (transform_object_t *obj, const char *format) {
    if (obj && format) {
//        MUTEX_LOCK (obj->lock);
        do {
            if (obj->originator && I (String)->equalWild (obj->originator->format,format)) {
                transform_object_t *found = obj->originator;
                obj->originator = NULL;
//                MUTEX_UNLOCK (obj->lock);
                return found;
            }
        } while ((obj = obj->originator));
//        MUTEX_UNLOCK (obj->lock);
    }
    return NULL;
}

static void
saveTransformObject (transform_object_t *obj) {
    if (obj) {
        DEBUGP (DDEBUG,"saveTransformObject","marking (%s) to be saved [meaning, someone else will clean the data!]",obj->format);
        obj->save = TRUE;
    }
}

static void
destroyTransformObject (transform_object_t **objPtr) {

    if (objPtr) {
        transform_object_t *obj = *objPtr;
        if (obj) {
            DEBUGP (DDEBUG,"destroyTransformObject","looking to get rid of (%s) with %d accessors with (%s) originator",obj->format,obj->access, (obj->originator)?obj->originator->format:"no");
//            MODULE_LOCK ();
            MUTEX_LOCK (obj->lock);
//            DEBUGP (DDEBUG,"destroyTransformObject","GOT THE OBJECT LOCK!");
            if (obj->access && !--obj->access) {
                if (!obj->save) {
                    if (obj->data) {
                        if (obj->destroy) {
                            DEBUGP (DDEBUG,"destroyTransformObject","calling destroy function on (%s)",obj->format);
                            (*obj->destroy) (&obj->data);
                        } else if (obj->destructor && I_ACCESS (obj->destructor, Transformation)->free) {
                            DEBUGP (DDEBUG,"destroyTransformObject","calling [%s:Transformation]->free(%s)",obj->destructor->name,obj->format);
                            I_ACCESS (obj->destructor, Transformation)->free (obj);
                        } else {
                            free (obj->data);
                        }
                    }
                } else {
                    DEBUGP (DDEBUG,"destroyTransformObject","(%s) is protected!",obj->format);
                }
                
                if (obj->originator) {
//                    MODULE_UNLOCK ();
                    DEBUGP (DDEBUG,"destroyTransformObject","recursing to originator (%s)",obj->originator->format);
                    destroyTransformObject (&obj->originator);
                }
                
                MUTEX_UNLOCK (obj->lock);
                MUTEX_CLEANUP (obj->lock);
                free (obj);
                *objPtr = NULL;
            } else {
//                MODULE_UNLOCK ();
                MUTEX_UNLOCK (obj->lock);
            }
        }
    }
}

IMPLEMENT_INTERFACE (TransformObject) = {
    .new     = newTransformObject,
    .find    = findTransfromObject,
    .pop     = popTransformObject,
    .split   = splitTransformObject,
    .attach  = attachTransformObject,
    .save    = saveTransformObject,
    .destroy = destroyTransformObject
};

/*//////// TransformToken Interface Implementation //////////////////////////////////////////*/

static transform_token_t *
newTransformToken (transform_target_t *target, transform_object_t *obj) {
    transform_token_t *token = (transform_token_t *)calloc (1, sizeof (transform_token_t));
    if (token) {
        token->target = target;
        token->obj = obj;
    }
    return token;
}

static void
destroyTransformToken (transform_token_t **tokPtr) {
    if (tokPtr) {
        transform_token_t *token = *tokPtr;
        // do NOT destroy the transform_target or the transform_object
        free (token);
        *tokPtr = NULL;
    }
}

IMPLEMENT_INTERFACE (TransformToken) = {
    .new      = newTransformToken,
    .destroy  = destroyTransformToken
};

/*//////// TransformEngine Interface Implementation //////////////////////////////////////////*/

typedef struct __transform_chain {

    char     *format;
    boolean_t pseudo;
    char     *name;
    
    struct __transform_chain *next;
    
} transform_chain_t;

typedef struct {

    transform_chain_t *chain;
    parameters_t      *blueprint;
    
} transform_rule_t;

/*
 * The SYMBOL maps (pseudo) which expands to various rule ENTRIES
 */
typedef struct {
    
	char *symbol;
	array_t *entries;           /* list of transform_rule_t OR transform_target_t (used in TransformationMatrix) */
    
} transform_symbol_t;

/*
 * The POTENTIAL transformation available in a given MODULE
 */
typedef struct {
    
    char *from;
    char *to;
    module_t *module;
    boolean_t direct;           /* set to TRUE if only direct matches are allowed! */
    
} transform_potential_t;

static boolean_t
_addTransformRule (array_t *rules, const char *ruleString, parameters_t *blueprint) {
    if (rules && ruleString && blueprint) {
        list_t *chains = I (String)->tokenize (ruleString, "->");
        if (chains && I (List)->count (chains) >= 2) { /* a likely transformation rule! */
            transform_rule_t *rule = (transform_rule_t *)calloc (1,sizeof (transform_rule_t));
            if (rule) {
                transform_chain_t *last = NULL;
                list_item_t *item = I (List)->first (chains);
                while (item) {
                    transform_chain_t *chain = (transform_chain_t *)calloc (1,sizeof (transform_chain_t));
                    if (chain) {
                        /* check for == assignment */
                        char *format = NULL;
                        list_t *links = I (String)->tokenize (item->data, "==");
                        if (links && I (List)->count (links) >= 2) {
                            list_item_t *litem = NULL;
                            if ((litem = I (List)->first (links))) {
                                format = I (String)->copy (I (String)->trim ((const char *) litem->data));
                            }
                            if ((litem = I (List)->last (links))) {
                                chain->name = I (String)->copy (I (String)->trim ((const char *) litem->data));
                            }

                            if (I (List)->count (links) > 2) {
                                DEBUGP (DWARN,"_addTransformRule","multiple transform named assignments detected in rule, using [%s == %s] only!",format,chain->name);
                            }

                        } else {
                            format = I (String)->copy (I (String)->trim ((const char *) item->data));
                        }

                        /* done with using links */
                        I (List)->clear (links,TRUE);
                        I (List)->destroy (&links);
                        

                        if (format && strlen (format)) {
                            chain->format = format;

                            /* detect if pseudo symbol */
                            if (*format == '(' && format[strlen (format)-1] == ')')
                                chain->pseudo = TRUE;
                            
                            if (last) last->next = chain;
                            else rule->chain = chain;
                            last = chain;
                        } else {
                            /* empty format... prevent memory leakage */
                            free (chain);
                        }
                        
                    } else {
                        /* XXX - ERROR, handle this??? */
                    }
                    item = item->next;
                }
                rule->blueprint = I (Parameters)->copy (blueprint);
                I (Array)->add (rules, rule);

                I (List)->clear (chains,TRUE);
                I (List)->destroy (&chains);
                
                return TRUE;
            }
        }
        I (List)->clear (chains,TRUE);
        I (List)->destroy (&chains);
    }
    return FALSE;
}

static array_t *
_getRulesFromConfiguration (configuration_t *conf) {
    array_t *rules = I (Array)->new ();
    if (conf) {
        category_list_t *categoryItem = conf->categories;
        while (categoryItem) {
            category_t *category = categoryItem->category;
            if (category) {
                category_t *duplicate = category;
                while (duplicate) {
                    _addTransformRule (rules, duplicate->name, I (Category)->getParameters (duplicate));
                    duplicate = duplicate->next;
                }
            }
            /* XXX - handle if addRule fails... */
            categoryItem = categoryItem->next;
        }
    }
    return rules;
}

static array_t *
_getAvailableTransforms (module_t *module) {
    array_t *transforms = I (Array)->new ();
    if (transforms && module) {
        if (module->transforms && I_ACCESS (module, Transformation)) {
            int32_t idx = 0;
            while (module->transforms[idx]) {
                char *transformRule = module->transforms[idx];
                list_t *chains = NULL;
                boolean_t directOnly = FALSE;
                if (strstr (transformRule,"->")) {
                    chains = I (String)->tokenize (transformRule, "->");
                } else if (strstr (transformRule,"=>")) {
                    chains = I (String)->tokenize (transformRule, "=>");
                    directOnly = TRUE;
                }
                /* XXX - here is where we should handle >> transform processing */

                
                if (chains && I (List)->count (chains) == 3) {
                    transform_potential_t *chaosEntry = (transform_potential_t *)calloc (1,sizeof (transform_potential_t));
                    if (chaosEntry) {
                        list_item_t *first = I (List)->first (chains);
                        list_item_t *last  = I (List)->last (chains);
                        chaosEntry->from = I (String)->copy (I (String)->trim ((const char *) first->data));
                        chaosEntry->to   = I (String)->copy (I (String)->trim ((const char *) last->data));
                        chaosEntry->direct = directOnly;
                        
                        DEBUGP (DDEBUG,"_getAvailableTransforms","found [%s -> %s] (%s) inside '%s'",chaosEntry->from,chaosEntry->to,directOnly?"only direct":"any",module->name);
                        chaosEntry->module = module;
                    } else {
                        DEBUGP (DERR,"_getAvailableTransforms","unable to allocate memory for chaosEntry!");
                    }
                    I (Array)->add (transforms, chaosEntry);
                } else {
                    DEBUGP (DERR,"_getAvailableTransforms","%s - is not a valid transformation rule!",transformRule);
                }
                idx++;
            }
        } else {
            DEBUGP (DERR,"_getAvailableTransforms","requested module does not support Transformation interface!");
        }
    }
    return transforms;
}

static void
_printTransformRules (array_t *rules) {
    if (rules) {
        int i = 0, count = I (Array)->count (rules);
        for (i = 0; i < count; i++) {
            transform_rule_t *rule = (transform_rule_t *) rules->items[i];
            transform_chain_t *chain = rule->chain;
            printf ("[%02d] ",i+1);
            while (chain) {
                printf ("%s {%s} -> ",chain->format,
                        (chain->pseudo?"pseudo":"real"));
                //(chain->name?chain->name:"anon"));
                // not sure what the above chain->name was for... but everything shows up as anon!
                chain = chain->next;
            }
            printf ("END\n");
            char *mystring = I (Parameters)->toString (rule->blueprint);
            if (mystring)
                printf ("%s", mystring);
        }
    }
}

/* main API routines */

static transform_engine_t *
newTransformEngine (configuration_t *conf) {
    transform_engine_t *engine = (transform_engine_t *)calloc (1,sizeof (transform_engine_t));
    if (engine) {
        if (conf) {
            engine->rules = _getRulesFromConfiguration (conf);
        } else {
            engine->rules = I (Array)->new ();
        }
        engine->chaos = _getAvailableTransforms (SELF); /* initial chaos support from BASIC transformations */
    }
    return engine;
}

/* TransformEngine::RuleProcessing */

static boolean_t
addTransformRule (transform_engine_t *engine, const char *ruleString, parameters_t *blueprint) {
    if (engine && engine->rules) {
        return _addTransformRule (engine->rules, ruleString, blueprint);
    }
    return FALSE;
}

static void
printTransformRules (transform_engine_t *engine) {
    if (engine && engine->rules)
        _printTransformRules (engine->rules);
}

/* TransformEngine::SymbolResolution */

static int __symbol_cmp (void *first, void *second) {
    if (!first || !second) return -1;
    return (!strncmp (((transform_symbol_t *)first)->symbol,(char *)second, strlen (second)));
}

static void __destroySymbolEntry (void *data) {
    transform_symbol_t *symbol = (transform_symbol_t *)data;
    if (symbol) {
        // XXX - we don't destroy contents in the entries ARRAY (need to see if this causes memory leaks...)
        I (Array)->destroy (&symbol->entries,NULL);
        free (symbol);
    }
}

/*
 * iterates through the rules as defined via configuration file or via manual addRule invokations
 *
 * detects (pseudo) symbols in the rules and builds the symbol table for expansion
 * when building the ruleset.
 *
 * returns: array list of symbols, else null.
 **/
static array_t *
_buildSymbolTable (array_t *rules) {
    array_t *symtbl = I (Array)->new ();
    if (symtbl && rules) {
        int i = 0, count = I (Array)->count (rules);
        for (i = 0; i < count; i++) {
            transform_rule_t *rule = (transform_rule_t *)rules->items[i];
            transform_chain_t *chain = rule->chain;
            while (chain) {
                /*
                 * if the particular chain FORMAT is a pseudo symbol and has mapping to another chain...
                 */
                if (chain->pseudo && chain->next) {
                    transform_symbol_t *symbol = (transform_symbol_t *) I (Array)->match (symtbl, chain->format, __symbol_cmp);
                    if (!symbol) {
                        symbol = (transform_symbol_t *)calloc(1,sizeof(transform_symbol_t));
                        symbol->symbol = chain->format;
                        symbol->entries = I (Array)->new ();
                        I (Array)->add (symtbl, symbol);
                    }
                    transform_rule_t *entry = (transform_rule_t *)calloc (1,sizeof (transform_rule_t));
                    entry->chain = chain->next;
                    entry->blueprint = rule->blueprint;
                    I (Array)->add (symbol->entries, entry);
                }
                chain = chain->next;
            }
        }
    }
    return symtbl;
}

/*
 * to be honest, I have absolutely no f*cking idea what all this __iterateXXX code does...
 *
 * Well, I *must* have known what I was doing back then... :/
 */
static array_t *__iterateChain (transform_chain_t *, array_t *);

static array_t *
__iterateSymbol (transform_chain_t *chain, array_t *symtbl) {
    /*
     * assumes that "chain" is defined
     */
    transform_symbol_t *symbol = I (Array)->match (symtbl,chain->format, __symbol_cmp);
    if (symbol) {
        array_t *newRules = I (Array)->new ();
        if (newRules) {
            int i = 0;
            do {
                transform_rule_t *rule = (transform_rule_t *)symbol->entries->items[i];
                array_t *nextRules = NULL;
                if (rule->chain->pseudo)
                    nextRules = __iterateSymbol (rule->chain,symtbl);
                else
                    nextRules = __iterateChain (rule->chain,symtbl);
                if (nextRules) {
                    int j = 0;
                    do {
                        transform_rule_t *nextRule = (transform_rule_t *)nextRules->items[j];
                        nextRule->blueprint = I (Parameters)->join (nextRule->blueprint, rule->blueprint);
                        I (Array)->add (newRules, nextRule);
                    } while ((++j < I (Array)->count (nextRules)));
                    I (Array)->destroy (&nextRules, NULL);
                } else {
                    /* XXX - handle this error? */
                    return NULL;
                }
            } while ((++i < I (Array)->count (symbol->entries)));

            return newRules;
        }
    } else {
        DEBUGP (DERR,"__iterateSymbol","unresolved symbol (%s) detected!", chain->format);
    }
    return NULL;
}

static array_t *
__iterateChain (transform_chain_t *chain, array_t *symtbl) {
    array_t *newRules = NULL, *nextRules = NULL;

    if (!chain) {               /* last element return condition */
        newRules = I (Array)->new ();
        if (newRules) {
            transform_rule_t *newRule = (transform_rule_t *)calloc (1, sizeof (transform_rule_t));
            if (newRule) {
                newRule->blueprint = I (Parameters)->new ();
                I (Array)->add (newRules, newRule);
                return newRules;
            }
            I (Array)->destroy (&newRules, NULL);
        }
        return NULL;
    }

    if (chain->pseudo)
        return __iterateSymbol (chain, symtbl);

    nextRules = __iterateChain (chain->next, symtbl);
    if (nextRules) {
        newRules = I (Array)->new ();
        if (newRules) {
            int i = 0;
            do {
                transform_rule_t  *nextRule = (transform_rule_t *)nextRules->items[i];
                transform_chain_t *newChain = (transform_chain_t *)calloc (1,sizeof (transform_chain_t));
                if (newChain) {
                    newChain->format = chain->format;
                    newChain->next = nextRule->chain;
                    nextRule->chain = newChain;
                    I (Array)->add (newRules, nextRule);
                } else {
                    DEBUGP (DERR,"__iterateChain","FATAL: OOM during copying chain!");
                    return NULL;
                }
            } while ((++i < I (Array)->count (nextRules)));
        }
        I (Array)->destroy (&nextRules, NULL);
    }
    return newRules;
}

/*
 * iterates through the rules as passed in via config file, and given the
 * symbol table, attempts to resolve & expand the symbols from the rules.
 *
 * returns: array of (rules array), else null.
 */
static array_t *
_buildTransformRuleSets (array_t *rules) {
    array_t *ruleSets = I (Array)->new ();
    if (ruleSets && rules) {
        array_t *symtbl = _buildSymbolTable (rules);
        int i = 0, count = I (Array)->count (rules);
        for (i = 0; i < count; i++) {
            transform_rule_t *rule = (transform_rule_t *)rules->items[i];
            transform_chain_t *chain = rule->chain;
            if (!chain->pseudo) { /* real data source */
                array_t *ruleSet = I (Array)->new ();
                array_t *newRules = __iterateChain (chain,symtbl);
                if (newRules) {
                    int j = 0;
                    do {
                        transform_rule_t *newRule = (transform_rule_t *)newRules->items[j];
                        newRule->blueprint = I (Parameters)->join (newRule->blueprint, rule->blueprint);
                        I (Array)->add (ruleSet,newRule);
                    } while ((++j < I (Array)->count (newRules)));
                    /* XXX - debugging... */
                    printf ("// ruleset\n");
                    _printTransformRules (ruleSet);
                    printf ("\n");
                }
                I (Array)->destroy (&newRules,NULL);
                I (Array)->add (ruleSets,ruleSet);
            }
        }
        I (Array)->destroy (&symtbl,__destroySymbolEntry);
    }
    return ruleSets;
}

/* TransformEngine::Explosion - expands RULESETs into actual transformation execution chains */

/*
 * note: both may contain wildcard characters!
 *
 * common case is NO MATCH.  coded accordingly.
 */
static int
compare_wildcard_strings(char *left, char *right) {
	if (*left=='*') {
		char *rstr = right;
		/* find the occurances on right string that has left+1 */
		while ((rstr = strchr(rstr,*(left+1)))){
			if (compare_wildcard_strings(left+1,rstr))
				return (1); /* yay! */
			rstr++; /* next possible match */
		}
	}
	if (*right=='*') {
		char *lstr = left;
		/* find the occurances on left string that has right+1 */
		while ((lstr = strchr(lstr,*(right+1)))){
			if (compare_wildcard_strings(lstr,right+1))
				return (1); /* yay! */
			lstr++; /* next possible match */
		}
	}
	if (*left != *right) return (0); /* nada */

	/* implicit *left == *right */
	if (*left == '\0') return (1); /* yay! */
	return compare_wildcard_strings(left+1,right+1);
}

static char *
combine_wildcard_strings(char *left, char *right) {
	char *result = NULL;
	if (!left||!right) return NULL;

	if (*left=='*') {
		char *rstr = right;
		/* find the occurances on right string that has left+1 */
		while ((rstr = strchr(rstr,*(left+1)))){
			result = combine_wildcard_strings(left+1,rstr);
			if (result){ /* PREPEND (right -> rstr) */
				size_t newsize  = (rstr-right)+strlen(result)+1;
				char *newstring = (char *)realloc(result,newsize);
				if (!newstring) {
					DEBUGP(0,"combine_wildcard_strings","OOM");
					free(result); return NULL;
				}
				memmove(newstring+(rstr-right),newstring,strlen(newstring)+1);
				memcpy(newstring,right,(rstr-right));
/*				DEBUG(2,fprintf(stderr,"new string is: %s\n",newstring)); */
				return (newstring);
			}
			rstr++; /* next possible match */
		}
	}
	if (*right=='*') {
		char *lstr = left;
		/* find the occurances on left string that has right+1 */
		while ((lstr = strchr(lstr,*(right+1)))){
			result = combine_wildcard_strings(lstr,right+1);
			if (result){ /* PREPEND (left -> lstr) */
				size_t newsize  = (lstr-left)+strlen(result)+1;
				char *newstring = (char *)realloc(result,newsize);
				if (!newstring) {
					DEBUGP(0,"combine_wildcard_strings","OOM");
					free(result); return NULL;
				}
				memmove(newstring+(lstr-left),newstring,strlen(newstring)+1);
				memcpy(newstring,left,(lstr-left));
/*				DEBUG(2,fprintf(stderr,"new string is: %s\n",newstring)); */
				return (newstring);
			}				
			lstr++; /* next possible match */
		}
	}
	if (*left != *right) return NULL; /* nada */

	/* implicit *left == *right */
	if (*left == '\0') {
		char *newstring = (char *)calloc(1,sizeof(char));
		newstring[0] = '\0';
		return (newstring);
	}
	
	result = combine_wildcard_strings(left+1,right+1);
	if (result) {
		char *newstring = (char *)realloc(result,sizeof(char *)*(strlen(result)+2));
		if (!newstring) {
			free(result); return NULL;
		}
		memmove(newstring+1,newstring,strlen(newstring)+1);
		newstring[0] = *left;
		return (newstring);
	}
	return NULL;
}

static int __potential_from_cmp(void *first, void *second) {
    if (first && second) {
        char *one = ((transform_potential_t *)first)->from;
        char *two = (char *)second;
        /* do something about wildcards here */
        return compare_wildcard_strings(one,two);
    }
    return 0;
}

static int __potential_to_cmp(void *first, void *second) {
    if (first && second) {
        char *one = ((transform_potential_t *)first)->to;
        char *two = (char *)second;
        /* do something about wildcards here */
        return compare_wildcard_strings(one,two);
    }
    return 0;
}

/*
 * look in the 'chaos' table for direct A->B match.
 *
 * returns candidates in PAST & FUTURE if passed in. (!NULL)
 * A->X goes to PAST   (left potentials)
 * X->B goes to FUTURE (right potentials)
 *
 * returns: transform_potential if there's a match, else NULL.
 **/
static transform_potential_t *
__directMatchCheck(array_t *chaos, char *from, char *to, array_t *past, array_t *future) {

    I_TYPE (Array) *I_ARRAY = I (Array); /* speed up array interface call access operations... */
	transform_potential_t *match       = NULL;
	transform_potential_t *left_match  = NULL;
	transform_potential_t *right_match = NULL;
	int ix = 0; int jx = 0;
    
	if (!chaos) return NULL;
	DEBUGP (DDEBUG,"__directMatchCheck","checking for: [%s->%s]",from,to);
	while((ix = I_ARRAY->find(chaos,from,ix,__potential_from_cmp))!=-1){
		left_match = (transform_potential_t *)chaos->items[ix];
		DEBUGP (DDEBUG,"__directMatchCheck","LEFT MATCH @ %d: [%s->%s]",ix,left_match->from,left_match->to);
		while((jx = I_ARRAY->find(chaos,to,jx,__potential_to_cmp))!=-1){
			right_match = (transform_potential_t *)chaos->items[jx];
			DEBUGP (DDEBUG,"__directMatchCheck","RIGHT MATCH @ %d: [%s->%s]",jx,right_match->from,right_match->to);
			if (ix == jx) { /* a solid match */
				match = left_match;
                I_ARRAY->clear(chaos,ix);
                I_ARRAY->cleanup(chaos);
				DEBUGP (DDEBUG,"__directMatchCheck","found: [%s->%s]",left_match->from,left_match->to);
				return (match);
			}
			if (ix < jx) break; /* advance the left side */
			/* implicit (ix > jx)... no solid match here */
			if (future)	I_ARRAY->add(future,right_match);
            I_ARRAY->clear(chaos,jx);  /* key to optimization!!! */
			jx++;
		}
		if (past && !left_match->direct) I_ARRAY->add(past,left_match);
		I_ARRAY->clear(chaos,ix); /* key to optimization!!! */
		ix++;
	}
	if ((jx != -1)&&(future)){
		I_ARRAY->add(future,right_match);
		I_ARRAY->clear(chaos,jx); /* key to optimization */
	}
	I_ARRAY->cleanup(chaos);
    if (past && future) {
        DEBUGP (DDEBUG,"__directMatchCheck","LEFT POTENTIAL LIST: %d  RIGHT POTENTIAL LIST: %d",past->num, future->num);
    }
	return (match);
}

/*
 * look in the 'chaos' table for lvals X rvals combination of direct
 * matches.
 *
 * first see if (right-side of lvals) == (left-side of rvals), in which
 * case there would be an indirect match between lval & rval, and result in
 * returning 'match_list' with 2 entries.
 *
 * if we're not so lucky, then we gotta go through each (right-side of lval
 * + left-side of rval) combination and see if there's a direct match!
 *
 * the combination that triggers match gets added into match_list like
 * thus:
 *
 * new match_list = lval + direct_match_result + rval
 *
 * And the process continues until we get something, or NULL.
 **/
static array_t *
__indirectRecurse(array_t *chaos, array_t *lvals, array_t *rvals, int lix, int rix) {
    
	if (lvals->num <= lix) return NULL;
	if (rvals->num <= rix) return __indirectRecurse(chaos,lvals,rvals,lix+1,0);
	{
        array_t *result = NULL;
		transform_potential_t *left  = (transform_potential_t *)lvals->items[lix];
		transform_potential_t *right = (transform_potential_t *)rvals->items[rix];
//		printf("LOOKING @: %s -> %s\n",left->datatype_out,right->datatype_in);
		/* test for the lucky indirect match */
		if (compare_wildcard_strings(left->to,right->from)){
			result = I (Array)->new();
			I (Array)->add(result,left);
			I (Array)->add(result,right);
            DEBUGP (DDEBUG,"__indirectRecurse","found: [%s->%s->%s]",left->from,left->to,right->to);
		}
		else { /* test for the semi-lucky direct match */
			array_t *left_potentials = I (Array)->new();
			array_t *right_potentials = I (Array)->new();
			transform_potential_t *match =
				__directMatchCheck(chaos, left->to,right->from,
								   left_potentials, right_potentials);
			if (match) {
				result = I (Array)->new();
				I (Array)->add(result,left);
				I (Array)->add(result,match);
				I (Array)->add(result,right);
			}
			else { /* let's move on to rest of list for 'lucky' matches */
				result = __indirectRecurse(chaos,lvals,rvals,lix,rix+1);
                if (!result) {
                    /* we're here ONLY when no lucky match check above is present! */
                    result = __indirectRecurse(chaos,left_potentials,right_potentials,0,0);
                }
			}

            I (Array)->destroy(&left_potentials,NULL);
            I (Array)->destroy(&right_potentials,NULL);
		}
        return (result);
	}
}

/*
 * The incantation that brings forth a spell for achieving the guidelines
 * set forth in ORDER from the pool of CHAOS that describes alternatives.
 *
 * I don't know how to describe it better!  really! (*grins*) - saint
 *
 * Okay... so for you laymen out there, this routine takes the
 * Transformation chain of a given RULE, and resolves the PATH it
 * needs to traverse in order to go from starting format all the way
 * to the final format based on all the available transformation potentials.
 */
static array_t *
_transformationMagic (transform_chain_t *order, array_t *chaos) {
    array_t *theWay = I (Array)->new ();
    if (theWay) {
        if (order && chaos) {   /* need to have BOTH :) */
            while (order && order->next) {
                array_t *nexus = I (Array)->clone (chaos); /* just a copy of CHAOS */
                array_t *past = I (Array)->new ();   /* list of all possibilities to the left */
                array_t *future = I (Array)->new (); /* list of all possibilities to the right */

                DEBUGP (DDEBUG,"_transformationMagic","resolving transformation rule for %s -> %s",order->format, order->next->format);
                
                if (nexus && past && future) { /* ah ha, now we're talking... */
                    transform_potential_t *step =
                        __directMatchCheck (nexus, order->format, order->next->format, past, future);
                    if (step) {
                        /* found a direct step to get from A -> B */
                        I (Array)->add (theWay, step);
                    } else {
                        array_t *course =
                            __indirectRecurse (nexus, past, future, 0, 0);
                        if (course) {
                            /* found indirect way to get from A -> B */
                            I (Array)->merge (&theWay,&course);
                        } else {
                            DEBUGP (DWARN,"_transformationMagic","No way to get from %s -> %s, the magic fizzles...",order->format, order->next->format);
                            I (Array)->destroy (&nexus,NULL);
                            I (Array)->destroy (&past,NULL);
                            I (Array)->destroy (&future,NULL);
                            I (Array)->destroy (&theWay,NULL);
                            break;
                        }
                    }
                }
                
                /* Poof, be gone... */
                I (Array)->destroy (&nexus,NULL);
                I (Array)->destroy (&past,NULL);
                I (Array)->destroy (&future,NULL);
                order = order->next;
            }
        } else {
            DEBUGP (DERR,"_transformationMagic","must provide BOTH order and choas!");
        }
    } else {
        DEBUGP (DERR,"_transformationMagic","OOM - trying to create theWay!");
    }
    return theWay;
}

static void __destroyTransformation (void *data) {
    transformation_t *xform = (transformation_t *)data;
    if (xform && xform->module)
        I_ACCESS (xform->module,Transformation)->destroy (&xform);
}

/*
 * When called, it resolves the RULES as loaded via Configuration file or via addRule calls
 * into an executable set of Transformation instance sequences.
 *
 * returns: transformation_matrix object
 */ 
static transformation_matrix_t *
resolveTransformations (transform_engine_t *engine) {
    if (engine && engine->rules) {
        array_t *executionSets = I (Array)->new ();
        if (executionSets) {
            array_t *ruleSets = _buildTransformRuleSets (engine->rules);
            if (ruleSets) {
                int i = 0, nRuleSets = I (Array)->count (ruleSets);
                for (i = 0; i < nRuleSets; i++) {
                    array_t *executionSet = I (Array)->new ();
                    if (executionSet) {
                        array_t *ruleSet = (array_t *)ruleSets->items[i]; /* implicitly assume that ruleSet is here! assert? */
                        int j = 0, nRules = I (Array)->count (ruleSet);
                        for (j = 0; j < nRules; j++) {
                            array_t *executionPath = I (Array)->new ();
                            if (executionPath) {
                                transform_rule_t *rule = (transform_rule_t *)ruleSet->items[j]; /* implicitly assume that a RULE is here, and that CHAIN is defined! assert? */
                                array_t *invocation = _transformationMagic (rule->chain, engine->chaos);
                                if (invocation) {
                                    int k = 0, nSteps = I (Array)->count (invocation);
                                    transformation_t *xform = NULL, *lastXform = NULL;

                                    /*
                                     * Yes, this is a bit of a funky algorithm here...
                                     * What we're trying to do is resolve any wildcard definitions so that based on the actual Transformation potentials discovered,
                                     * we utilize the most *precise* definition of the FORMAT as possible.
                                     *
                                     * For instance, if the invocation chain is as follows... 'Worker -> Fruit:*' and 'Fruit:Apple -> Apple:Pie'
                                     * Then, what we do is while looking at 'Worker -> Fruit:*', we take a peek at the next transformation potential
                                     * and actually resolve it as 'Worker -> Fruit:Apple' and 'Fruit:Apple -> Apple:Pie'.
                                     */ 
                                    do {
                                        transform_potential_t *now  = (transform_potential_t *)invocation->items[k]; /* implicitly assume that a POTENTIAL is here! assert? */
                                        char *trueFrom = now->from;
                                        char *trueTo   = now->to;
                                        if (k + 1 < nSteps) {
                                            transform_potential_t *next = (transform_potential_t *)invocation->items[k+1];
                                            if (next) {
                                                trueTo = combine_wildcard_strings (now->to, next->from);
                                            }
                                        } else {
                                            // combine with the actual last rule format
                                            transform_chain_t *myChain = rule->chain;
                                            while (myChain->next) {
                                                myChain = myChain->next;
                                            }
                                            trueTo = combine_wildcard_strings (now->to, myChain->format);
                                        }
                                        
                                        if (lastXform) {
                                            trueFrom = lastXform->to;
                                        } else {
                                            /* first entry, we can infer from the explicit rule->chain's first format */
                                            trueFrom = rule->chain->format;
                                        }
                                        
                                        /* here we have DIRECT access to the module to initiate a new Transformation object! */
                                        DEBUGP (DDEBUG,"resolveTransformations","instantiating Transformation at %s for (%s -> %s) @ %p",now->module->name, trueFrom, trueTo, I_ACCESS (now->module,Transformation)->new);
                                        xform = I_ACCESS (now->module, Transformation)->new (trueFrom, trueTo, rule->blueprint);
                                        if (xform) {
                                            /*
                                             * For now, let's handle the BASIC transformations here...
                                             *
                                             * There should be some sort of multi-pass capability but don't want to think too much atm... 
                                             */
                                            DEBUGP (DDEBUG,"resolveTransformations","instantiated (%s -> %s) transformation!", trueFrom, trueTo);
                                            if (xform->module == SELF) { /* reference to self means we should do BASIC transformation */
                                                transform_object_t *result = I (Transformation)->execute (xform, NULL);
                                                if (result) {
                                                    DEBUGP (DDEBUG,"resolveTransformations","performed BASIC transformation!");
                                                    if (!strncasecmp (result->format, "transform:engine:chaos", strlen ("transform:engine:chaos"))) {
                                                        array_t *chaos = (array_t *)result->data;
                                                        if (chaos) {
                                                            I (Array)->merge (&engine->chaos, &chaos);
                                                            result->data = NULL; /* the merge operation destroys this array reference */
                                                        }
                                                    } else {
                                                        DEBUGP (DERR,"resolveTransformations","transformation execution returned unsupported data format: %s",result->format);
                                                    }
                                                    I (TransformObject)->destroy (&result);
                                                }
                                                I_ACCESS (xform->module,Transformation)->destroy (&xform);
                                            } else {
                                                I (Array)->add (executionPath, xform);
                                            }
                                        } else {
                                            DEBUGP (DERR,"resolveTransformations","unable to instantiate xform for (%s -> %s) transformation!",trueFrom, trueTo);
                                            I (Array)->destroy (&executionPath, __destroyTransformation);
                                            break;
                                        }
                                        lastXform = xform; /* it's okay if xform is NULL */
                                    } while (++k < nSteps);
                                    
                                } else {
                                    DEBUGP (DERR,"resolveTransformations","transformation magic failed for RULESET #%d, RULE #%d. skipping...",i,j);
                                }
                                if (I (Array)->count (executionPath))
                                    I (Array)->add (executionSet, executionPath);
                            } else {
                                DEBUGP (DERR,"resolveTransformations","OOM while trying to create executionPath array at RULESET #%d, RULE #%d. skipping...",i,j);
                            }
                        }
                        if (I (Array)->count (executionSet))
                            I (Array)->add (executionSets, executionSet);
                    } else {
                        DEBUGP (DERR,"resolveTransformations","OOM while trying to create executionSet array at RULESET #%d!  Skipping this RULESET...",i)
                    }
                }
            } else {
                DEBUGP (DERR,"resolveTransformations","Cannot build transform RULESETs from RULEs!");
            }
            /* proper return path */
            return I (TransformationMatrix)->new (executionSets);
        } else {
            DEBUGP (DERR,"resolveTransformations","OOM while trying to create executionSets array!");
        }
    } else {
        DEBUGP (DERR,"resolveTransformations","Engine is not properly loaded with rules!");
    }
    return NULL;
}

static void
destroyTransformEngine (transform_engine_t **ePtr) {
    /*
     * XXX - need to implement transform engine destructor!!!
     *
     * Later, when multiple transform engines may be in play during execution
     */
}

IMPLEMENT_INTERFACE (TransformEngine) = {
    .new        = newTransformEngine,
    .addRule    = addTransformRule,
    .resolve    = resolveTransformations,
    .printRules = printTransformRules,
    .destroy    = destroyTransformEngine
};

/*//////// Transformation Interface Implementation //////////////////////////////////////////*/

static transformation_t *
newBasicTransformation (const char *from, const char *to, parameters_t *blueprint) {
    if (from && to && blueprint) {
        transformation_t * xform = (transformation_t *)calloc (1, sizeof (transformation_t));
        if (xform) {
            /* validate from & to */
            if (!strncasecmp (from, "transform:module", strlen ("transform:module")) &&
                !strncasecmp (to,   "transform:engine", strlen ("transform:engine"))) {
                xform->module = SELF;
                xform->blueprint = I (Parameters)->copy (blueprint);
                xform->from = strdup (from);
                xform->to   = strdup (to);
            } else {
                DEBUGP (DERR,"newBasicTransformation", "transformation %s -> %s is not supported!", from, to);
                free (xform);
                xform = NULL;
            }
        }
        return xform;
    }
    return NULL;
}

static transform_object_t *
executeBasicTransformation (transformation_t *xform, transform_object_t *in) {
    /*
     * assumes that all structures inside XFORM has been properly initialized.
     *
     * does not care about IN.
     */
	if (xform) {
        /*
         * for 'transform:module -> transform:engine'
         *
         * This execution dynamically loads a MODULE and retrieves all available TRANSFORMATIONS
         * 
         * returns: a new TransformObject containing an array of CHAOS entries
         */
        if (!strncasecmp (xform->from, "transform:module", strlen ("transform:module")) &&
            !strncasecmp (xform->to,   "transform:engine", strlen ("transform:engine"))) {
            
            char *moduleName = I (Parameters)->getValue (xform->blueprint,"module");
            if (moduleName) {
                module_t *module = I (DynamicLoader)->load (moduleName);
                if (module) {
                    array_t *transforms = _getAvailableTransforms (module);
                    if (transforms && I (Array)->count (transforms)) {
                        transform_object_t *obj = I (TransformObject)->new ("transform:engine:chaos",transforms);
                        if (obj) obj->destructor = SELF;
                        return obj;
                    } else {
                        DEBUGP (DERR,"executeBasicTransformation","could not find any Transformations in requested module, %s!",moduleName);
                        I (DynamicLoader)->unload (module);
                    }
                } else {
                    DEBUGP (DERR,"executeBasicTransformation","cannot find requested module, %s",moduleName);
                }
            } else {
                DEBUGP (DERR,"executeBasicTransformation","insufficient argument: need to pass in 'module' parameter!");
            }
        }
        
    }
	return NULL;
}

static void
destroyBasicTransformation (transformation_t **tPtr) {
	if (tPtr && *tPtr) {
        transformation_t *xform = *tPtr;
        if (xform) {
            I (Parameters)->destroy (&xform->blueprint);
            free (xform->from);
            free (xform->to);
            free (xform);
            *tPtr = NULL;
        }
	}
}

/*
 * XXX - does this belong here???
 */
static void
freeBasicTransformObject (transform_object_t *obj) {
	if (obj) {
        if (!strncasecmp (obj->format, "transform:engine:chaos", strlen ("transform:engine:chaos"))) {
            if (obj->data) {
                array_t *chaos = (array_t *)obj->data;
                I (Array)->destroy (&chaos,NULL);
                obj->data = NULL;
            }
        }
    }
}

IMPLEMENT_INTERFACE (Transformation) = {
	.new       = newBasicTransformation,
	.destroy   = destroyBasicTransformation,
	.execute   = executeBasicTransformation,
	.free      = freeBasicTransformObject
};

/*//////// TransformationMatrix Interface //////////////////////////////////////////*/

typedef struct {

    transformation_t *xform;
    array_t *nextTargets;
    
} transformation_matrix_entry_t;

static transformation_matrix_t *
newTransformationMatrix (array_t *sets) {
    transformation_matrix_t *matrix = (transformation_matrix_t *)calloc (1, sizeof (transformation_matrix_t));
    if (matrix) {
        if (sets && I (Array)->count (sets)) {
            matrix->targets = I (Array)->new ();
            if (matrix->targets) {
                /*
                 * here build the mapping
                 *
                 * We want to iterate through each transformation and find
                 * out all the (x,y,z) targets that the xform->from format links to
                 */
                array_t *symtbl = I (Array)->new ();
                if (symtbl) {
                    int i,j,k;
                    array_t *set, *rule;
                    transformation_t *xform;
                    // first build the symtbl
                    DEBUGP (DINFO,"newTransformationMatrix","building the transformation matrix symbol table...");
                    for (i = 0; (set = (array_t *) I (Array)->get (sets,i)); i++) {
                        for (j = 0; (rule = (array_t *) I (Array)->get (set,j)); j++) {
                            for (k = 0; (xform = (transformation_t *) I (Array)->get (rule,k)); k++) {
                                transform_target_t *target = (transform_target_t *)calloc (1,sizeof (transform_target_t));
                                DEBUGP (DDEBUG,"newTransformationMatrix","(%d,%d,%d) - [%s -> %s]",i,j,k, xform->from, xform->to);
                                if (target && xform->from) {
                                    transform_symbol_t *symbol = (transform_symbol_t *) I (Array)->match (symtbl, xform->from, __symbol_cmp);
                                    if (!symbol) {
                                        symbol = (transform_symbol_t *)calloc(1,sizeof(transform_symbol_t));
                                        symbol->symbol = xform->from;
                                        symbol->entries = I (Array)->new ();
                                        I (Array)->add (symtbl, symbol);
                                    }
                                    target->set = i;
                                    target->rule = j;
                                    target->chain = k;
                                    I (Array)->add (symbol->entries, target);
                                    I (Array)->add (matrix->targets, target);
                                }
                            }
                        }
                    }

                    // then create the matrix entries
                    DEBUGP (DINFO,"newTransformationMatrix","creating matrix entries...");
                    for (i = 0; (set = (array_t *) I (Array)->get (sets,i)); i++) {
                        for (j = 0; (rule = (array_t *) I (Array)->get (set,j)); j++) {
                            for (k = 0; (xform = (transformation_t *) I (Array)->get (rule,k)); k++) {
                                transformation_matrix_entry_t *matrixEntry = (transformation_matrix_entry_t *)calloc (1,sizeof (transformation_matrix_entry_t));
                                if (matrixEntry && xform->to) {
                                    transform_symbol_t *symbol = (transform_symbol_t *) I (Array)->match (symtbl, xform->to, __symbol_cmp);
                                    if (symbol) {
                                        transform_target_t *target = NULL;
                                        int s = 0;
                                        matrixEntry->nextTargets = I (Array)->new ();
                                        while ((target = (transform_target_t *)I (Array)->get (symbol->entries,s++))) {
                                            if (target->set != i) continue; /* ignore targets that do not belong in the current set */
                                            if (target->chain != k+1) continue; /* ignore chains that are not immediate decendents of current chain order */

                                            /* XXX - special skip logic to prevent duplicate transformations
                                             * if any target is from a previous rule, we ignore this entire xform
                                             */
                                            if (target->rule < j) {
                                                DEBUGP (DDEBUG,"newTransformationMatrix","skipping duplicate transform for (%s -> %s)",xform->from,xform->to);
                                                I_ACCESS (xform->module, Transformation)->destroy (&xform);
                                                break;
                                            }
                                        }
                                    }
                                    matrixEntry->xform = xform;
                                }
                                I (Array)->replace (rule,k,matrixEntry);
                            }
                        }
                    }
                    matrix->sets = sets;

                    // finally merge the common transformation paths
                    DEBUGP (DINFO,"newTransformationMatrix","merging common transformation path...");
                    for (i = 0; (set = (array_t *) I (Array)->get (sets,i)); i++) {
                        for (j = 0; (rule = (array_t *) I (Array)->get (set,j)); j++) {
                            transformation_matrix_entry_t *matrixEntry = NULL;
                            for (k = 0; (matrixEntry = (transformation_matrix_entry_t *) I (Array)->get (rule,k)); k++) {
                                if (matrixEntry->xform && matrixEntry->xform->to) {
                                    transform_symbol_t *symbol = (transform_symbol_t *) I (Array)->match (symtbl, matrixEntry->xform->to, __symbol_cmp);
                                    if (symbol) {
                                        transform_target_t *target = NULL;
                                        int s = 0;
                                        while ((target = (transform_target_t *)I (Array)->get (symbol->entries,s++))) {
                                            if (target->set != i) continue; /* ignore targets that do not belong in the current set */
                                            if (target->chain != k+1) continue; /* ignore chains that are not immediate decendents of current chain order */
                                            
                                            // validate that the target points to existing transformation (not a duplicate!)
                                            if (I (TransformationMatrix)->lookup (matrix, target)) {
                                                DEBUGP (DDEBUG,"newTransformationMatrix","adding next target (%d,%d,%d)",target->set,target->rule,target->chain);
                                                I (Array)->add (matrixEntry->nextTargets,target);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                }
                I (Array)->destroy (&symtbl,__destroySymbolEntry);
            } else {
                free (matrix);
                matrix = NULL;
            }
        } else {
            DEBUGP (DERR,"newTransformationMatrix","cannot create a matrix from empty set!");
            free (matrix);
            matrix = NULL;
        }
    }
    return matrix;
}

static inline void __destroyMatrixTarget (void *data) { free (data); }

static void
destroyTransformationMatrix (transformation_matrix_t **mPtr) {
    if (mPtr) {
        transformation_matrix_t *matrix = *mPtr;
        if (matrix) {
            int i = 0, j = 0, k = 0;
            array_t *set, *rule;
            transformation_matrix_entry_t *matrixEntry;
            while ((set = (array_t *) I (Array)->get (matrix->sets,i++))) {
                while ((rule = (array_t *) I (Array)->get (set,j++))) {
                    while ((matrixEntry = (transformation_matrix_entry_t *) I (Array)->get (rule,k++))) {
                        if (matrixEntry->xform && matrixEntry->xform->module) 
                            I_ACCESS (matrixEntry->xform->module, Transformation)->destroy (&matrixEntry->xform);
                        I (Array)->destroy (&matrixEntry->nextTargets,NULL);
                        free (matrixEntry);
                    }
                    I (Array)->destroy (&rule,NULL);
                }
                I (Array)->destroy (&set,NULL);
            }
            I (Array)->destroy (&matrix->sets,NULL);
            I (Array)->destroy (&matrix->targets,__destroyMatrixTarget);
        }
    }
}

static list_t *
getFeederTargetsFromTransformationMatrix (transformation_matrix_t *matrix) {
    if (matrix) {
        list_t *feederTargets = I (List)->new ();
        if (feederTargets) {
            int i = 0;
            transform_target_t *target = NULL;
            while ((target = (transform_target_t *) I (Array)->get (matrix->targets,i++))) {
                // put a feeder into the feeder queue
                if (target->rule == 0 && target->chain == 0) {
                    transformation_t *xform = I (TransformationMatrix)->lookup (matrix,target);
                    if (xform && I (String)->equal (xform->from,"transform:feeder")) {
                        I (List)->insert (feederTargets, I (ListItem)->new (target));
                    }
                }
            }
        }
        return feederTargets;
    }
    return NULL;
}

static list_t *
getLinkerTargetsFromTransformationMatrix (transformation_matrix_t *matrix) {
    if (matrix) {
        list_t *targets = I (List)->new ();
        if (targets) {
            int i = 0;
            transform_target_t *target = NULL;
            while ((target = (transform_target_t *) I (Array)->get (matrix->targets,i++))) {
                // put a linker into the feeder queue
                if (target->chain == 0) {
                    transformation_t *xform = I (TransformationMatrix)->lookup (matrix,target);
                    if (xform && !I (String)->equal (xform->from,"transform:feeder")) {
                        I (List)->insert (targets, I (ListItem)->new (target));
                    }
                }
            }
        }
        return targets;
    }
    return NULL;
}

static list_t *
getNextTargetsFromTransformationMatrix (transformation_matrix_t *matrix, transform_target_t *target) {
    if (matrix && target) {
        list_t *nextTargets = I (List)->new ();
        if (nextTargets) {
            array_t *rules = (array_t *) I (Array)->get (matrix->sets,target->set);
            if (rules) {
                array_t *chains = (array_t *) I (Array)->get (rules,target->rule);
                if (chains) {
                    transformation_matrix_entry_t *matrixEntry = (transformation_matrix_entry_t *)I (Array)->get (chains,target->chain);
                    if (matrixEntry) {
                        int i = 0;
                        transform_target_t *target = NULL;
                        while ((target = (transform_target_t *)I (Array)->get (matrixEntry->nextTargets,i++))) {
                            I (List)->insert (nextTargets,I (ListItem)->new (target));
                        }
                    }
                }
            }
        }
        return nextTargets;
    }
    return NULL;
}

static transformation_t *
lookupTransformationMatrix (transformation_matrix_t *matrix, transform_target_t *target) {
    transformation_t *transform = NULL;
    if (matrix && target) {
        array_t *rules = (array_t *) I (Array)->get (matrix->sets,target->set);
        if (rules) {
            array_t *chains = (array_t *) I (Array)->get (rules,target->rule);
            if (chains) {
                transformation_matrix_entry_t *matrixEntry = (transformation_matrix_entry_t *)I (Array)->get (chains,target->chain);
                if (matrixEntry) {
                    transform = matrixEntry->xform;
                }
            }
        }
    }
    return transform;
}

static int
countTransformationMatrixSets (transformation_matrix_t *matrix) {
    if (matrix) {
        return I (Array)->count (matrix->sets);
    }
    return 0;
}

static int
countTransformationMatrixRules (transformation_matrix_t *matrix) {
    if (matrix) {
        int i = 0, numRules = 0;
        array_t *set;
        while ((set = (array_t *) I (Array)->get (matrix->sets,i++))) {
            numRules += I (Array)->count (set);
        }
        return numRules;
    }
    return 0;
}

static void
printTransformationMatrix (transformation_matrix_t *matrix) {
    if (matrix) {
        int i,j,k;
        array_t *set, *rule;
        transformation_matrix_entry_t *entry;
        
        for (i = 0; (set = (array_t *) I (Array)->get (matrix->sets,i)); i++) {
            printf ("// matrix execution set #%d\n",i);
            for (j = 0; (rule = (array_t *) I (Array)->get (set,j)); j++) {
                printf ("[%02d] ",j);
                for (k = 0; (entry = (transformation_matrix_entry_t *) I (Array)->get (rule,k)); k++) {
                    transformation_t *xform = entry->xform;
                    if (entry->xform) {
                        printf ("(%s -> %s) ",xform->from,xform->to);
                    } else {
                        printf ("(skip) ");
                    }
                }
                printf ("\n");
            }
            printf ("\n");
        }
    }
}

IMPLEMENT_INTERFACE (TransformationMatrix) = {
	.new              = newTransformationMatrix,
    .getFeederTargets = getFeederTargetsFromTransformationMatrix,
    .getLinkerTargets = getLinkerTargetsFromTransformationMatrix,
    .getNextTargets   = getNextTargetsFromTransformationMatrix,
    .lookup           = lookupTransformationMatrix,
    .countSets        = countTransformationMatrixSets,
    .countRules       = countTransformationMatrixRules,
    .print            = printTransformationMatrix,
	.destroy          = destroyTransformationMatrix
};

/*//////// Transform Object Queue Interface //////////////////////////////////////////*/

static transform_object_queue_t *
newTransformObjectQueue (uint32_t maxSize) {
    return (transform_object_queue_t *) I (Queue)->new (maxSize/255, 255);
}

static transform_object_t *
getTransformObject (transform_object_queue_t *tQueue) {
    return (transform_object_t *)I (Queue)->get ((cqueue_t *) tQueue);
}

static boolean_t 
putTransformObject (transform_object_queue_t *tQueue, transform_object_t *object) {
	if (tQueue && object){
        return I (Queue)->put ((cqueue_t *)tQueue, object);
    }
    return FALSE;
}

static void
destroyTransformObjectQueue (transform_object_queue_t **qPtr) {
	if (qPtr) {
		transform_object_queue_t *tQueue = *qPtr;
		if (tQueue) {
            transform_object_t *old;
            I (Queue)->disable ((cqueue_t *) tQueue);
            while ((old = (transform_object_t *) I (Queue)->drop ((cqueue_t *) tQueue))) {
                I (TransformObject)->destroy (&old);
            }
            I (Queue)->destroy (&tQueue);
			*qPtr = NULL;
		}
	}
}

IMPLEMENT_INTERFACE (TransformObjectQueue) = {
	.new     = newTransformObjectQueue,
	.get     = getTransformObject,
	.put     = putTransformObject,
	.destroy = destroyTransformObjectQueue
};

/*//////// Transform Token Queue Interface //////////////////////////////////////////*/

static transform_token_queue_t *
newTransformTokenQueue (uint32_t maxSize, int timeout_ms) {
    transform_token_queue_t *queue = I (Queue)->new (maxSize/255, 255);
    if (timeout_ms > 0)
        I (Queue)->setTimeout (queue, timeout_ms);
    if (timeout_ms < 0)
        I (Queue)->setBlocking (queue, TRUE);

    return (transform_token_queue_t *) queue;
}

static transform_token_t *
getTransformToken (transform_token_queue_t *tQueue) {
    return (transform_token_t *)I (Queue)->get ((cqueue_t *) tQueue);
}

static boolean_t 
putTransformToken (transform_token_queue_t *tQueue, transform_token_t *token) {
	if (tQueue && token) {
        return I (Queue)->put ((cqueue_t *) tQueue, token);
	}
	return FALSE;
}

static void
destroyTransformTokenQueue (transform_token_queue_t **qPtr) {
	if (qPtr) {
		transform_token_queue_t *tQueue = *qPtr;
		if (tQueue) {
            transform_token_t *old;
            I (Queue)->disable ((cqueue_t *) tQueue);
            while ((old = (transform_token_t *) I (Queue)->drop ((cqueue_t *) tQueue))) {
                I (TransformToken)->destroy (&old);
            }
            I (Queue)->destroy (&tQueue);
			*qPtr = NULL;
		}
	}
}

static uint32_t
countTransformTokenQueue (transform_token_queue_t *tQueue) {
    if (tQueue)
        return tQueue->pending;
    return 0;
}

IMPLEMENT_INTERFACE (TransformTokenQueue) = {
	.new     = newTransformTokenQueue,
	.get     = getTransformToken,
	.put     = putTransformToken,
    .count   = countTransformTokenQueue,
	.destroy = destroyTransformTokenQueue
};
