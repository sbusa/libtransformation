#ifndef __gumbo_parser_H__
#define __gumbo_parser_H__

#include <corenova/interface.h>
#include <corenova/data/array.h>
#include <gumbo.h>

typedef struct 
{	
	int  buffer_lenth;			//Length of the input buffer
	char *buffer;			   //pointer to the input buffer
	GumboOutput *output;	   //Gumbo parser output
	array_t *match_refs;	   // stores the positions of the matches 
    MUTEX_TYPE lock;           /* makes operations thread-safe */    
} gumboParser_t;

// This structure is used for storing the  offset position & length of the match tags.
typedef struct 
{
	GumboNode *node;
	char *start;
	uint32_t length;
	char *name;
} tag_t;

/*//////// MODULE DECLARATION //////////////////////////////////////////*/

DEFINE_INTERFACE (Gumbo)
{
	gumboParser_t*  (*new)          (char *buf, int size);	
	void      		(*destroy)      (gumboParser_t *);	
	char *    		(*toString)     (gumboParser_t *);
	void  	   		(*match)        (gumboParser_t *, GumboNode *node, GumboTag tagName, char *attribute, char *text);
	void       		(*remove)       (gumboParser_t *);    
    char *          (*getAttrValue) (GumboNode *, const char *attrname);
};

#endif
