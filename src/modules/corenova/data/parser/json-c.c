#include <corenova/source-stub.h>
#include <corenova/data/parser/jsonparser.h>

THIS = {
    .version = "2.0",
    .author = "Peter K. Lee <saint@corenova.com>",
    .description = "This module provides a set of json-c based json parser specific operations.",
    .implements = LIST("JsonParser"),
    .requires = LIST("corenova.data.array", "corenova.data.list", "corenova.data.md5")
};

#include <corenova/data/list.h>
#include <corenova/data/array.h>
#include <corenova/data/md5.h>
#include <corenova/data/string.h>

/*//////// MODULE CODE //////////////////////////////////////////*/
static jsonParser_t * newJsonParser(char *data, int len)
{
	DEBUGP(DDEBUG, "jsonParser", "newJsonParser");
	jsonParser_t *jsonParser = (jsonParser_t *)calloc (1,sizeof (jsonParser_t));
	if (jsonParser)
	{
        // Build json parser data out of the data supplied to this function
	}   
	return NULL;
}


static void destroyJsonParser(jsonParser_t *jsonParser)
{
	if (jsonParser)
	{	
		// Make sure that memory allocated locally is freed here 
	}
}

static char* _getBuffer(jsonParser_t *jsonParser)
{
    /* Return modified buffer here */ 
	return NULL;
}


/*

static array_t *
_matchContent(const json_t *json, const char *match) {
    if (json && match) {
		array_t *arrNodes = NULL;
     
     	// match logic here
     	// Loop through nodes and build an array of node
     	// locations which match the url

        return arrNodes;
    }
    return NULL;
}

static json_node_t *
_addNode(json_t *json, json_node_t *new) {
    if (json && new) {
        // should we support addNodeBefore & addNodeAfter??
        // Add logic here
    }
	return new;
}

static json_node_t *
_removeNode(json_t *json, json_node_t *old) {
    if (json && old) {
        // should we support addNodeBefore & addNodeAfter??
        // Add logic here
    }
	return old;
}

static char *
_translateJson(const json_t *with, char *obuf) {
	if (with && obuf) {
		char *copy = NULL;

		return copy;
	}   
	return NULL;
}
*/


IMPLEMENT_INTERFACE(JsonParser) = {
    .new = newJsonParser,
    .toString = _getBuffer,
    .destroy = destroyJsonParser,
};
