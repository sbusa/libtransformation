#include <corenova/source-stub.h>
#include <corenova/data/parser/gumboparser.h>

THIS = {
    .version = "2.0",
    .author = "Peter K. Lee <saint@corenova.com>",
    .description = "This module provides a set of gumbo html5 parser specific operations.",
    .implements = LIST("GumboParser"),
    .requires = LIST("corenova.data.array", "corenova.data.list", "corenova.data.md5")
};

#include <corenova/data/list.h>
#include <corenova/data/array.h>
#include <corenova/data/md5.h>
#include <corenova/data/string.h>

/*//////// MODULE CODE //////////////////////////////////////////*/
static gumboParser_t * newGumboParser(char *data, int len)
{
	DEBUGP(DDEBUG, "gumboParser", "newGumboParser");
	gumboParser_t *gumboParser = (gumboParser_t *)calloc (1,sizeof (gumboParser_t));
	if (gumboParser)
	{
        // Build gumbo tree out of the data supplied
		gumboParser->buffer_lenth=len+1;
		gumboParser->buffer=(char *)calloc(gumboParser->buffer_lenth,sizeof(char));
		if(gumboParser->buffer== NULL)
		{
			return NULL;
		}
		memcpy(gumboParser->buffer,data,len);
		gumboParser->buffer[gumboParser->buffer_lenth] = '\0';

		gumboParser->output=gumbo_parse_with_options(&kGumboDefaultOptions,gumboParser->buffer,gumboParser->buffer_lenth);

		MUTEX_SETUP (gumboParser->lock);
	}   
	return NULL;
}


static void destroyGumboParser(gumboParser_t *gumboParser)
{
	if (gumboParser)
	{
		gumbo_destroy_output(&kGumboDefaultOptions, gumboParser->output);
		if(gumboParser->buffer)
		{
			free(gumboParser->buffer);
			gumboParser->buffer = NULL;
		}
		gumboParser->buffer_lenth=0;
		MUTEX_CLEANUP (gumboParser->lock);
	}
}


static char* _getBuffer(gumboParser_t *gumboParser)
{
	return (gumboParser->buffer);
}



/*

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

		return copy;
	}   
	return NULL;
}
*/


IMPLEMENT_INTERFACE(GumboParser) = {
    .new = newGumboParser,
    .toString = _getBuffer,
    .destroy = destroyGumboParser,
};
