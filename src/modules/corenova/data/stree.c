#include <corenova/source-stub.h>

THIS = {
	.version     = "2.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "Binary Search Tree",
	.implements  = LIST ("STree","TreeNode")
};

#include <corenova/data/stree.h>

static tree_node_t *
newTreeNode (void *data) {

    tree_node_t *node = (tree_node_t *) calloc (1, sizeof (tree_node_t));

    if (node) {
    
	node->data = data;
	node->status = STATUS_NORMAL;
	
    }
	
    return node;
    
}

static void
destroyTreeNode (tree_node_t **itemPtr) {

    if (itemPtr) {

	tree_node_t *item = *itemPtr;

	if (item) {
	
	    if(item->data)
		free(item->data);

	    free (item);
	    *itemPtr = NULL;
	    
	}

    }
    
}

IMPLEMENT_INTERFACE (TreeNode) = {

	.new = newTreeNode,
	.destroy = destroyTreeNode
	
};


static stree_t *
_newSTree (int32_t (*compar)(const void *, const void *)) {

    stree_t *tree = (stree_t *) calloc (1, sizeof (stree_t));
    
    if (tree) {

	MUTEX_SETUP (tree->lock);
	
	tree->compar = compar;
	
    }

    return tree;
    
}

static void
_destroy (stree_t **tree) {

    if(tree && *tree) {
    
	I (STree)->clear(*tree);
	
	MUTEX_CLEANUP((*tree)->lock);

	free(*tree);

	*tree = NULL;
	    
    }

}

static inline tree_node_t *
_insert (stree_t *tree, tree_node_t *new) {

    int32_t c = 0;
    
    if(!new)
	return NULL;
	
    MUTEX_LOCK (tree->lock);
    
    new->serial = tree->serial;

    tree_node_t *n = tree->root;

    if(!n) {

	tree->root = new;
	new->parent = NULL;
	tree->count = 1;
	tree->size = 1;

	MUTEX_UNLOCK (tree->lock);
	return new;
    
    }    
    
    while(n) {
    
/*	if(n->status == STATUS_DELETED) {

	    // should re-use node if possible
	
	
	} else { */
	
	    if(n->serial != tree->serial) {
	    
		if((new->left = n->left))
		    new->left->parent = new;
		    
		if((new->right = n->right))
		    new->right->parent = new;
		
		if(n == tree->root) {
		
		    // has no parent
		    
		    tree->root = new;
		    
		
		} else
		
		if(n->parent->left == n) {
		
		    n->parent->left = new;
		
		} else {
		
		    n->parent->right = new;
		
		}
		
		I (TreeNode)->destroy(&n);
		tree->count++;
		tree->size++;
		MUTEX_UNLOCK(tree->lock);
		return new;
	    
	    }
    
    	    c = tree->compar(n->data, new->data);
	    
	    if(c < 0) {

		if(!n->right) {
		
		    n->right = new;
		    new->parent = n;
		    tree->count++;
		    tree->size++;

		    MUTEX_UNLOCK (tree->lock);
		    return n->right;
		
		}		

		n = n->right;
		
	    } else if(c > 0) {

		if(!n->left) {
		
		    n->left = new;
		    new->parent = n;
		    tree->count++;
		    tree->size++;

		    MUTEX_UNLOCK (tree->lock);
		    return n->left;
		
		}		

		n = n->left;
	    
	    } else {
	
		MUTEX_UNLOCK (tree->lock);
		return n; 		// can't accomodate duplicates, and don't have to
	    
	    }
	    
	    
//	}
	    
    }
    
    return NULL;
    
}

static inline tree_node_t *
_find (stree_t *tree, void *data) {

    int32_t c = 0;

    MUTEX_LOCK (tree->lock);

    tree_node_t *n = I (STree)->root(tree);

    if(!n) {

	MUTEX_UNLOCK (tree->lock);
	return NULL;
    
    }    
    
    while(n) {

    
/*	if(n->status == STATUS_DELETED) {

	    // should re-use node if possible
	
	
	} else { */
	
	    if(tree->serial != n->serial) {
	    
		MUTEX_UNLOCK (tree->lock);
		return NULL;
	    
	    }
    
    	    c = tree->compar(n->data, data);
	    
	    if(c < 0) {

		if(!n->right) {
		
		    MUTEX_UNLOCK (tree->lock);
		    return NULL;
		
		}		

		n = n->right;
		
	    } else if(c > 0) {

		if(!n->left) {
		
		    MUTEX_UNLOCK (tree->lock);
		    return NULL;
		
		}		

		n = n->left;
	    
	    } else {
	
		MUTEX_UNLOCK (tree->lock);
		return n; // found it!
	    
	    }
	    
	    
//	}
	    
    }
    
    return NULL;
    
}

static void **
_serialize(stree_t *tree, uint32_t *size) {

    if(!tree || tree->count == 0) {
		*size = 0;
		return NULL;
	}

    MUTEX_LOCK (tree->lock);

    void **data = calloc(tree->count, sizeof(void *));
    
    (*size) = 0;
    
    tree_iter_t *iter = I (STree)->first(tree);
    
    tree_node_t *n = iter->node;

    while(n) {
    
	data[(*size)++] = n->data;
	
	n = I (STree)->next(iter);
    
    }
    
    free(iter);

    MUTEX_UNLOCK (tree->lock);
    return data;

}

static void 
_visit(tree_node_t *node, void *data, uint32_t *count, uint32_t width) {

    memcpy(data+((*count)++)*width, node->data, width);

    if(node->left) {
        _visit(node->left, data, count, width);
    }
    
    if(node->right) {
        _visit(node->right, data, count, width);    
    }

}

static void *
_serialize2(stree_t *tree, uint32_t *size, uint32_t width) {

    if(!tree || tree->count == 0) {
    
	*size = 0;
	return NULL;
	
    }
	
    MUTEX_LOCK (tree->lock);

    void *data = calloc(tree->count, width);
    
    (*size) = 0;
    
    tree_iter_t *iter = I (STree)->first(tree);
    
    tree_node_t *n = iter->node;
    
    while(n) {
    
	memcpy(data+((*size)++)*width, n->data, width);
	
	n = I (STree)->next(iter);
    
    }
    
    free(iter);
    
//    _visit(tree->root, data, size, width);
    
    MUTEX_UNLOCK (tree->lock);

    return data;

}

static tree_node_t *
_remove(stree_t *tree, tree_node_t *old) {

    if (old) {
    
	MUTEX_LOCK (tree->lock);
	
	if(old == tree->root) { // can't remove root
	
	    old->status = STATUS_DELETED;
	
	} else if(!old->parent) {
	
	     // debug case: node not in tree
	
	} else
	
	if(!old->right || !old->left) { // if only single child
	
	    if(old->parent->left == old) { // child on the left side ?
	    
		if(old->left) {
	    
    		    old->parent->left = old->left;
		    
		} else if(old->right) {
		
		    old->parent->left = old->right;
		
		} else {  // have no kids at all
		
		    old->parent->left = NULL;
		
		}
		
		if(old->serial == tree->serial)
		    tree->count--;
		    
		tree->size--;
		
	    } else { // child on the right
	    
		if(old->left) {
	    
    		    old->parent->right = old->left;
		    
		} else if(old->right) {
		
		    old->parent->right = old->right;
		
		} else {
		
		    old->parent->right = NULL;
		
		}
		
		if(old->serial == tree->serial)
		    tree->count--;
		    
		tree->size--;
	    
	    }
	
	} else {
	
	    old->status = STATUS_DELETED;  // lazy removal: no removal for nodes with 2 children
					    // iteration should skip such nodes
	
	}
	
	MUTEX_UNLOCK (tree->lock);

    }

    return old;
}

static inline tree_node_t *
_root(stree_t *tree) { return tree->root; }

static inline uint32_t
_count (stree_t *tree) { 

    if(!tree)
	return 0;
    
    return tree->count; 

}

static inline uint32_t
_size (stree_t *tree) { 

    if(!tree)
	return 0;

    return tree->size; 

}

static inline tree_iter_t *
_first(stree_t *tree) {

    tree_iter_t *iter = calloc(1, sizeof(tree_iter_t));
    
    if(!iter)
	return NULL;
	
    iter->node = tree->root;
    iter->count = 0;
    iter->size = tree->count;
    iter->serial = tree->serial;
    
    if(iter->node->status == STATUS_DELETED || tree->serial != iter->node->serial)	// seek to the next valid node
	I (STree)->next(iter);
    
    return iter;
    
}

static inline tree_node_t *
_next(tree_iter_t *iter) {

    
//    if(iter->count>=iter->size-1)
//	return NULL;

    if(iter->node->left && iter->serial == iter->node->left->serial) {
    
	iter->count++;    
	iter->node = iter->node->left;
	
	if(iter->node->status == STATUS_DELETED)
	    I (STree)->next(iter);	// some small recursion here
	
	return iter->node;
    
    } else 
    
    if(iter->node->right != NULL && iter->serial == iter->node->right->serial) {
    
	iter->count++;
	iter->node = iter->node->right;

	if(iter->node->status == STATUS_DELETED)
	    I (STree)->next(iter);	// some small recursion here

	return iter->node;
    
    } else {
/*    
	while(iter->node->parent && !iter->node->right && iter->node->parent->right != iter->node) {
    
	    if(iter->node->parent->parent == NULL && iter->node == iter->node->parent->right)
		    return NULL;		// got to the root coming from the right side
		
	    iter->node = iter->node->parent;
	    
	}
*/

	tree_node_t *prev = iter->node;

	while(iter->node->parent && (iter->node->right == NULL || iter->node->right == prev || iter->serial != iter->node->serial)) {
    
	    prev = iter->node;
		
	    iter->node = iter->node->parent;

	    if(iter->node->parent == NULL && (iter->node->right == prev || iter->node->right == NULL))
		    return NULL;		// got to the root coming from the right side

	    
	}


    }
    
    if(iter->node->right && iter->serial == iter->node->right->serial) {
    
	iter->count++;    
	iter->node = iter->node->right;

	if(iter->node->status == STATUS_DELETED)
	    I (STree)->next(iter);	// some small recursion here

	return iter->node;
	
    }

    return NULL;
    
}

static inline void
_expire(stree_t *tree) {

    if(!tree)
	return;

    tree->serial++;
    tree->count = 0;

}

static void
_clear(stree_t *tree) {

    if(!tree)
	return;

    MUTEX_LOCK (tree->lock);

    if(tree->size > 0) {
	
	tree_node_t *n = tree->root;
	    
	while(n) {
	    
	    if(!n->left && !n->right) {
		
	        if(!n->parent) {
		
		    I (TreeNode)->destroy(&tree->root);
		    tree->count = 0;
		    tree->size = 0;
		    break;		    
		    
		} else
		
		if(n->parent->left == n) {

		    n = n->parent;		
		    I (TreeNode)->destroy(&n->left);
		    
		} else
		    
		if(n->parent->right == n) {

		    n = n->parent;
		    I (TreeNode)->destroy(&n->right);
		    
		}
			
	    } else
		
	    if(n->left) {

		n = n->left;
		
	    } else if(n->right) {

		n = n->right;
		
	    } else {

	        n = n->parent;
		
	    }
		
	}
	
    }
    
    MUTEX_UNLOCK((tree)->lock);
	    
}

IMPLEMENT_INTERFACE (STree) = {
	.new          = _newSTree,
	.insert       = _insert,
	.find         = _find,
	.remove       = _remove,
	.root         = _root,
	.count        = _count,
	.size         = _size,
	.next         = _next,
	.first        = _first,
	.destroy      = _destroy,
	.serialize    = _serialize,
	.serialize2   = _serialize2,
	.clear        = _clear,
	.expire       = _expire
};

