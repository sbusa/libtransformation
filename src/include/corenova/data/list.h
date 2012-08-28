#ifndef __list_H__
#define __list_H__

#include <corenova/interface.h>

typedef struct _listItem {

	void *data;					/* node can contain anything. */
	
	struct _listItem *next;
	struct _listItem *prev;

} list_item_t;

DEFINE_INTERFACE (ListItem) {
	list_item_t *(*new)     (void *data);
	void         (*destroy) (list_item_t **);
};

typedef struct {

	list_item_t *head;
	list_item_t *tail;
	
	uint32_t count;

	MUTEX_TYPE lock;			/* make the list operation thread-safe */
	
} list_t;

DEFINE_INTERFACE (List) {
	list_t        *(*new)          (void);
	list_item_t   *(*insertAfter)  (list_t *, list_item_t *new, list_item_t *next);
	list_item_t   *(*insertBefore) (list_t *, list_item_t *new, list_item_t *before);
	list_item_t   *(*insert)       (list_t *, list_item_t *new);
	list_item_t   *(*remove)       (list_t *, list_item_t *old);
	list_item_t   *(*pop)          (list_t *); /* removes an entry from head of list */
	list_item_t   *(*drop)         (list_t *); /* removes an entry from tail of list */
	list_item_t   *(*first)        (list_t *);
	list_item_t   *(*last)         (list_t *);
	list_item_t   *(*next)         (list_item_t *);
	list_item_t   *(*prev)         (list_item_t *);
	uint32_t       (*count)        (list_t *);
	/*
	  NOTE: destroy function not implemented on purpose.

	  You should manually pop all the entries from the list, and deal with
	  the list items & data that the items contains manually.

	  After this, just free (list_t *);
	  
	  
	  UPDATE: need destroy function anyway
	  
	  We figured that lock has to be cleaned, or
	  that thing will leak memory...
	  
	*/
	void	      (*destroy)      (list_t **);
	void          (*clear)        (list_t *, boolean_t);
	
};

typedef struct _listIterator {

	list_item_t *item;
	list_t *list;

} list_iterator_t;

DEFINE_INTERFACE (ListIterator) {

    list_iterator_t *(*new) (list_t *);
    void             (*destroy) (list_iterator_t **);
    list_item_t     *(*next) (list_iterator_t *);
    void             (*remove) (list_iterator_t *);
    void             (*reset) (list_iterator_t *);
    list_item_t     *(*getItem) (list_iterator_t *);

};

#endif
