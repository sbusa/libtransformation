#ifndef __search_tree_H__
#define __search_tree_H__

#include <corenova/interface.h>

typedef struct _treeNode {

	void *data;
	
	struct _treeNode *parent;	// double-linked
	struct _treeNode *left;		// binary
	struct _treeNode *right;
	
	unsigned char status;
	unsigned char serial;
	
#define STATUS_UNDEF	0
#define STATUS_DELETED	1		// lazy removal
#define STATUS_NORMAL	2


} tree_node_t;

DEFINE_INTERFACE (TreeNode) {

	tree_node_t *(*new)     (void *data);
	void         (*destroy) (tree_node_t **);

};

typedef struct _iter {

#define	LEVEL_ORDER     1
#define	PRE_ORDER       2
#define	POST_ORDER      3

#define DIR_UP		1
#define DIR_RIGHT	2
#define DIR_DOWN	3
#define DIR_LEFT	4

        char type;
	char dir;
	uint32_t count;
	uint32_t size;
        tree_node_t *node;
	unsigned char serial;

} tree_iter_t;

typedef struct {

	tree_node_t *root;
	int32_t (*compar)(const void *, const void *); // comparator function
	uint32_t count;
	uint32_t size;
	unsigned char serial;

	MUTEX_TYPE lock;			// thread-safe
	
} stree_t;

DEFINE_INTERFACE (STree) {
	stree_t       *(*new)          (int32_t (*compar)(const void *, const void *));
	tree_node_t   *(*insert)       (stree_t *, tree_node_t *new);
	tree_node_t   *(*find)         (stree_t *, void *data);
	tree_node_t   *(*remove)       (stree_t *, tree_node_t *old);
	tree_node_t   *(*root)         (stree_t *);
	tree_iter_t   *(*first)        (stree_t *);
	tree_node_t   *(*next)         (tree_iter_t *);
	tree_node_t   *(*prev)         (tree_iter_t *);
	uint32_t       (*count)        (stree_t *);
	uint32_t       (*size)         (stree_t *);
	void	       (*destroy)      (stree_t **);
	void	       (*clear)        (stree_t *);	
	void	       (*expire)       (stree_t *);	
	void         **(*serialize)    (stree_t *tree, uint32_t *size);
	void          *(*serialize2)   (stree_t *tree, uint32_t *size, uint32_t width);
	
};

#endif
