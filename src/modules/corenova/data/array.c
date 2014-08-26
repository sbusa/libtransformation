#include <corenova/source-stub.h>

THIS = {
	.version     = "1.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "Provide ordered array based list data structure for storage & retrieval.",
	.implements  = LIST ("Array")
};

#include <corenova/data/array.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

/*//////// API ROUTINES //////////////////////////////////////////*/


static array_t *
newArray (void) {
    array_t *array = (array_t *)calloc(1,sizeof(array_t));
    if (array) {
        MUTEX_SETUP (array->lock);
    }
	return array;
}

static void
destroyArray (array_t **list, array_del_func del) {
    if (list && *list) {
        array_t *l = *list;
        if (del) 
            while (I (Array)->count (l)) { I (Array)->remove (l,del); } /* delete everything */
        if (l->items) free (l->items);
        MUTEX_CLEANUP (l->lock);
        free (l);
        *list = NULL;
    }
}

static void 
deleteArray (array_t *list, int index) {
	if (list && index < list->num) {
		MUTEX_LOCK (list->lock);
		if ((list->num - 1) != index)
		memmove(&list->items[index], &list->items[index + 1], sizeof(void *) * (list->num - 1 - index));		
		list->num --;
		MUTEX_UNLOCK (list->lock);
	}
}

static int
countArray (array_t *list) {
    if (list) {
        return list->num;
    }
    return 0;
}

static int
addArrayItem (array_t *list, void *item) {
    if (list && item) {
        MUTEX_LOCK (list->lock);
        if ((list->items = realloc(list->items, sizeof(void *)*(list->num+1)))) {
            memset(&list->items[list->num], 0, sizeof(void *)); // clear new ptr
            list->items[list->num] = item;
            list->num++;
            MUTEX_UNLOCK (list->lock);
            return (list->num);
        }
        MUTEX_UNLOCK (list->lock);
    }
    return 0;
}

static void
removeArrayItem (array_t *list, array_del_func del) {
    if (list && list->num && list->items && list->items[list->num-1]) {
        MUTEX_LOCK (list->lock);
        if (del) (*del)(list->items[list->num-1]);
        else free (list->items[list->num-1]);
        list->items[--list->num] = NULL;
        MUTEX_UNLOCK (list->lock);
    }
}

static void *
getArrayItem (array_t *list, int index) {
    if (list && list->num && index >= 0 && index < list->num) {
        return list->items[index];
    }
    return NULL;
}

static void *
firstArrayItem (array_t *list) {
    return getArrayItem (list,0);
}

static void *
lastArrayItem (array_t *list) {
    return getArrayItem (list,countArray (list) - 1);
}

static void *
replaceArrayItem (array_t *list, int index, void *with) {
    if (list && list->num && list->items && index >= 0 && list->num > index) {
        void *prev = list->items[index];
        list->items[index] = with;
        return prev;
    }
    return NULL;
}

/*
 * when provided a specific index, clears the pointer.
 *
 * Yes, seems pointless, but it has its uses... see below function :)
 * 
 * returns: address of cleared pointer
 */
static void *
clearArrayItem (array_t *list, int index) {
    if (list && list->num && list->items && index >= 0 && list->num > index) {
        void *ptr = list->items[index];
        list->items[index] = NULL;
        return ptr;
    }
    return NULL;
}

/*
 * Go through the list and make a new list w/o the cleared items/elements
 *
 * Basically, you can clear random items (using function above) but
 * this function actually frees up the memory space
 *
 * returns: nothing.
 */
static void
cleanupArray (array_t *list) {
	int ix = 0; int real_num = 0;
	if (!list||!list->num||!list->items) return; /* nothing to do */
	do {
		if (list->items[ix]) real_num++;
	} while (++ix < list->num);
	ix = 0;
	if (real_num == list->num) return;
	else {
		int jx = 0;
		void **new_items = calloc(real_num,sizeof(void *));
		if (!new_items) return; /* just keep the list as is */

        MUTEX_LOCK (list->lock);
		do {
			if (list->items[ix])
				new_items[jx++] = list->items[ix];
		} while (++ix < list->num);
		/* final touch */
		free(list->items);
		list->items = new_items;
		list->num = real_num;
        MUTEX_UNLOCK (list->lock);
	}
}

/*
 * COMPARES the 2 input list using the supplied comparison function
 *
 * returns: number of matches from front->back
 */
static int
compareArrays (array_t *one, array_t *two, array_cmp_func cmp) {
	int ix = 0, num_match = 0;
    if (one && two && cmp) {
        for (ix=0; ix < one->num; ix++){
            if (!one->items[ix]||!two->items[ix]) break;
            if (!(*cmp)(one->items[ix],two->items[ix])) break;
            num_match++;
        }
    }
    return num_match;
}

/*
 * LOOKS for the matching condition w/in it's data storage.
 * specify the starting ix.
 *
 * returns: (index) of match, else -1
 */
static int
findInArray (array_t *list, void *key, int start_ix, array_cmp_func cmp) {
  int ix = -1;
  if (list && key && start_ix >= 0 && cmp) {
      MUTEX_LOCK (list->lock);
      for (ix=start_ix; ix < list->num; ix++){
          if (!list->items[ix]) continue; /* null item? */
          if ((*cmp)(list->items[ix],key)) break;
      }
      MUTEX_UNLOCK (list->lock);
      if (ix < list->num) return ix;
  }
  return -1;
}

/*
 * more convenient function of find above where we start from index of 0
 *
 * returns: pointer to the matching item
 */
static void *
matchInArray (array_t *list, void *key, array_cmp_func cmp) {
	int ix = I (Array)->find(list,key,0,cmp);
	if (ix >= 0) return list->items[ix];
	return NULL;
}

/*
 * SPLITS the list at the specified index, generating two new arrays
 * The location of the index is where the 2nd list will start!
 * Note: the original array pointer should NOT be used afterwards!
 *
 * returns: TRUE or FALSE
 */
static boolean_t
splitArray (array_t *orig, int ix, array_t **first, array_t **second) {
    if (orig && orig->num > ix) {
        if ((*second = I (Array)->new ())) { /* created a new array */
            // handle second_half FIRST
            (*second)->num = orig->num - ix; // not that great, but ok
            if (((*second)->items = malloc(sizeof(void *)*(*second)->num))){
                DEBUGP(DINFO,"split","%d|%d",ix,orig->num-ix);
                // move over the pointers of original list at ix to the second list
                memcpy((*second)->items, &orig->items[ix], sizeof(void *)*(*second)->num);

                // now just re-size the original list to fit the first list
                (*first) = orig;
                (*first)->num = ix;

                MUTEX_LOCK ((*first)->lock);
                if (((*first)->items = realloc(orig->items, sizeof(void *)*ix))){
                    // bingo! (even if everything fails, orig is untouched)
                    // else, original is resized.
                    MUTEX_UNLOCK ((*first)->lock);
                    return TRUE;
                } else {
                    DEBUGP (DERR,"split","cannot allocate memory for first array!");
                    I (Array)->destroy (second, NULL);
                }
                MUTEX_UNLOCK ((*first)->lock);
            } else {
                DEBUGP (DERR,"split","cannot allocate memory for second array!");
                I (Array)->destroy (second, NULL);
            }
        }
    }
    return FALSE;
}

/*
 * MERGES the lists into TO, destroying WITH
 *
 * returns: # new list items on success, else 0
 */
static int
mergeArrays(array_t **to, array_t **with) {
    if (to && with && *to && *with && (*with)->num) {
        MUTEX_LOCK ((*to)->lock);
        if (((*to)->items =
             realloc((*to)->items,
                     (sizeof(void *)*(*to)->num) + (sizeof(void *)*(*with)->num)))) {
            memmove(&(*to)->items[(*to)->num],(*with)->items,sizeof(void *)*(*with)->num);
            (*to)->num += (*with)->num;
            I (Array)->destroy (with,NULL);
            MUTEX_UNLOCK ((*to)->lock);
            return (*to)->num;
        } 
        MUTEX_UNLOCK ((*to)->lock);
    }
    return 0;
}

/*
 * CLONES the original list, returning a copy
 *
 * returns: the copy, else NULL
 */
static array_t *
cloneArray (const array_t *orig) {
    if (orig) {
        array_t *temp = I (Array)->new ();
        if (temp) {
            temp->items = calloc(orig->num,sizeof(void *));
            if (temp->items) {
                memcpy(temp->items, orig->items, sizeof(void *)*(orig)->num);
                temp->num = orig->num;
                return (temp);
            }
            I (Array)->destroy (&temp,NULL);
        }
    }
    return NULL;
}

IMPLEMENT_INTERFACE (Array) = {
	.new          = newArray,
    .destroy      = destroyArray,
    .delete       = deleteArray,
    .count        = countArray,
    .add          = addArrayItem,
    .remove       = removeArrayItem,
    .get          = getArrayItem,
    .first        = firstArrayItem,
    .last         = lastArrayItem,
    .replace      = replaceArrayItem,
    .clear        = clearArrayItem,
    .cleanup      = cleanupArray,
    .compare      = compareArrays,
    .find         = findInArray,
    .match        = matchInArray,
    .split        = splitArray,
    .merge        = mergeArrays,
    .clone        = cloneArray
};
