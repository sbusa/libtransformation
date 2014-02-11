#include <corenova/source-stub.h>

THIS = {
    .version = "2.0",
    .author = "Peter K. Lee <saint@corenova.com>",
    .description = "This module provides a set of gumbo html5 parser specific operations.",
    .implements = LIST("Gumbo"),
    .requires = LIST("corenova.data.array", "corenova.data.list", "corenova.data.md5")
};

#include <corenova/data/list.h>
#include <corenova/data/array.h>
#include <corenova/data/md5.h>
#include <corenova/data/string.h>

/*//////// MODULE CODE //////////////////////////////////////////*/
static gumbo_t *
newGumboParser(char *data) {

	DEBUGP(DDEBUG, "gumbo", "newGumboParser");
	gumbo_t *gumboParser = (gumbo_t *)calloc (1,sizeof (gumbo_t));
	if (gumbo) {
        // Build gumbo tree out of the data supplied
		return gumboParser;
	}   
	return NULL;
}


static array_t *
_matchContent(const gumbo_t *gumbo, const char *match) {
    if (gumbo && match) {
		array_t *arrNodes = NULL;
     
     	// match logic here
     	// Loop through nodes and build an array of node
     	// locations which match the url

        return arrNodes;
    }
    return NULL;
}

static gumbo_node_t *
_addNode(gumbo_t *gumbo, gumbo_node_t *new) {
    if (gumbo && new) {
        // should we support addNodeBefore & addNodeAfter??
        // Add logic here
    }
	return new;
}

static gumbo_node_t *
_removeNode(gumbo_t *gumbo, gumbo_node_t *old) {
    if (gumbo && old) {
        // should we support addNodeBefore & addNodeAfter??
        // Add logic here
    }
	return old;
}

static char *
_translateGumbo(const gumbo_t *with, char *obuf) {
	if (with && obuf) {
		char *copy = NULL;
	    /* Translate modified gumbo back the new data, 
	     * might need string operations here for 
	     * allocation/modification of the original buffer - obuf, 
	     * need to allocate new buffer - copy */
		return copy;
	}   
	return NULL;
}


static void
destroyGumboParser(gumbo_t **ptr) {
	if (ptr) {
		gumbo_t *gumboParser = *ptr;
		if (gumboParser) {

			/* Free gumbo struct members here */

			free(gumboParser);
			*ptr = NULL;
		}
	}
}


IMPLEMENT_INTERFACE(Gumbo) = {
    .new = newGumboParser,
    .match = _matchContent,
    .add = _addNode,
    .remove = _removeNode,
    .toString = _translateGumbo,
    .destroy = destroyGumboParser,
};
