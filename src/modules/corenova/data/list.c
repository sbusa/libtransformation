#include <corenova/source-stub.h>

THIS = {
	.version     = "2.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "Provide linked list data structure for storage & retrieval.",
	.implements  = LIST ("List", "ListItem", "ListIterator")
};

#include <corenova/data/list.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

static list_item_t *
newListItem (void *data) {
	list_item_t *node = (list_item_t *) calloc (1, sizeof (list_item_t));
	if (node)
		node->data = data;
	return node;
}

static void
destroyListItem (list_item_t **itemPtr) {
	if (itemPtr) {
		list_item_t *item = *itemPtr;
		if (item) {
			/* NOTE: DOES NOTE FREE DATA! THIS IS BY DESIGN! */
			free (item);
			*itemPtr = NULL;
		}
	}
}

IMPLEMENT_INTERFACE (ListItem) = {
	.new = newListItem,
	.destroy = destroyListItem
};

/* Common List operators */

static list_t *
_newList (void) {
	list_t *list = (list_t *) calloc (1, sizeof (list_t));
	if (list) {
		MUTEX_SETUP (list->lock);
	}
	return list;
}

static void
_destroy (list_t **list) {
    if(list && *list) {
        if(I (List)->count(*list) > 0) {
            list_item_t *i;
            while((i = I (List)->pop(*list))!=NULL)
                I (ListItem)->destroy(&i);
        }
        MUTEX_CLEANUP((*list)->lock);
        free(*list);
        *list = NULL;
    }
}

static void
_clear (list_t *list, boolean_t withData) {
    list_item_t *item;
    if(list) {
        while((item = I (List)->pop(list))) {
            if(withData && item->data) {
                free(item->data);
            }
            I (ListItem)->destroy(&item);
        }
    }
}

static list_item_t *
_insertAfter (list_t *list, list_item_t *new, list_item_t *after) {
	if (new) {
		MUTEX_LOCK (list->lock);
		after = after ? after : list->tail;
		if (after) {
			new->next = after->next;
			new->prev = after;
			after->next = new;
			if (new->next)
				new->next->prev = new;
			else
				list->tail = new;
		} else {
			list->head = list->tail = new;
		}
		list->count++;
		MUTEX_UNLOCK (list->lock);
	}
	return new;
}

static list_item_t *
_insertBefore (list_t *list, list_item_t *new, list_item_t *before) {
	if (new) {
		MUTEX_LOCK (list->lock);
		before = before ? before : list->head;
		if (before) {
			new->prev = before->prev;
			new->next = before;
			before->prev = new;
			if (new->prev)
				new->prev->next = new;
			else
				list->head = new;
		} else {
			list->head = list->tail = new;
		}
		list->count++;
		MUTEX_UNLOCK (list->lock);
	}
	return new;
}

static inline list_item_t *
_insert (list_t *list, list_item_t *new) {
	return I (List)->insertAfter (list, new, NULL);
}

static list_item_t *
_remove (list_t *list, list_item_t *old) {
	if (old) {
		MUTEX_LOCK (list->lock);
		if (old->next)
			old->next->prev = old->prev;
		else
			list->tail = old->prev;

		if (old->prev)
			old->prev->next = old->next;
		else
			list->head = old->next;

		list->count--;
		MUTEX_UNLOCK (list->lock);
	}
	return old;
}

static list_item_t *
_pop (list_t *list) {
	return I (List)->remove (list,list->head);
}

static list_item_t *
_drop (list_t *list) {
	return I (List)->remove (list,list->tail);
}

static inline list_item_t *
_first (list_t *list) { return list->head; }

static inline list_item_t *
_last (list_t *list) { return list->tail; }

static inline uint32_t
_count (list_t *list) { return list->count; }

static inline list_item_t *
_next (list_item_t *node) {
	if (node) return node->next;
	return NULL;
}

static inline list_item_t *
_prev (list_item_t *node) {
	if (node) return node->prev;
	return NULL;
}

/* XXX - peter 5/16/09
 *
 * We should invest in writing a good sorting algorithm for this list
 * data type.
 *
 * If not integrated in this module, then as a wrapper module that
 * would take a list_t object and provide various sorting facilities.
 */

IMPLEMENT_INTERFACE (List) = {
	.new          = _newList,
	.insert       = _insert,
	.insertAfter  = _insertAfter,
	.insertBefore = _insertBefore,
	.remove       = _remove,
	.pop          = _pop,
	.drop         = _drop,
	.first        = _first,
	.last         = _last,
	.count        = _count,
	.next         = _next,
	.prev         = _prev,
	.destroy      = _destroy,
	.clear        = _clear
};

static list_iterator_t *
_newIterator(list_t *list) {
    list_iterator_t *iter = NULL;
    if(list) {
        list_item_t *item = I (List)->first(list);
        if(item) {
            iter = malloc(sizeof(list_iterator_t));
            iter->list = list;
            iter->item = item;
        }
    }
    return iter;
}

static void
_destroyIterator(list_iterator_t **iter) {
    if(iter && *iter) {
        free(*iter);
        *iter = NULL;	    
    }
}

static list_item_t*
_nextIterator(list_iterator_t *iter) {
    if(iter) {
        iter->item = I (List)->next(iter->item);
        return iter->item;
    }
    return NULL;
}

static void
_resetIterator(list_iterator_t *iter) {
    if(iter) {
        iter->item = I (List)->first(iter->list);    
    }
}

static inline list_item_t*
_getItemIterator(list_iterator_t *iter) {
    if(iter) {
        return iter->item;
    }
    return NULL;
}

static void
_removeIterator(list_iterator_t *iter) {
    if(iter && iter->item) {
        list_item_t *item = I (List)->next(iter->item);
        I (List)->remove(iter->list, iter->item);
        I (ListItem)->destroy(&iter->item);
        iter->item = item;
    }
}
    
IMPLEMENT_INTERFACE (ListIterator) = {
    .new               = _newIterator,
    .destroy           = _destroyIterator,
    .next              = _nextIterator,
    .remove            = _removeIterator,
    .reset             = _resetIterator,
    .getItem           = _getItemIterator
};

