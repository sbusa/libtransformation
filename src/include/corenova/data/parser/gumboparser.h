#ifndef __gumbo_parser_H__
#define __gumbo_parser_H__

#include <corenova/interface.h>
#include <corenova/data/array.h>
#include "gumbo.h"

typedef struct 
{	
	int  buffer_lenth;			//Length of the input buffer
	char *buffer;			   //pointer to the input buffer
	GumboOutput* output;	   //Gumbo parser output
	GumboNode *currentnode;    //internal place holder variable : used for tag/attribute searching.
	array_t *match_refs;	   // stores the positions of the matches 
    MUTEX_TYPE lock;           /* makes operations thread-safe */    
} gumboParser_t;

//enum for HTML TAGs. Same Tag values to be used as mentioned in Gumbo.h file.
typedef enum 
{
	GUMBOPARSER_TAG_A=39,	
}GumboParserTag;

// This structure is used for storing the  offset position & length of the match tags.
typedef struct 
{
	int offset;
	int length;		
}tagReferences_t;

/*//////// MODULE DECLARATION //////////////////////////////////////////*/

DEFINE_INTERFACE (GumboParser)
{
	gumboParser_t*  (*new)     (char *, int length);	
	void      		(*destroy) (gumboParser_t *);	
    char *    		(*toString) (gumboParser_t *);
    int  	   		(*match)(gumboParser_t *gumboParser,GumboParserTag tagName, char *attribute, char *text);
    void       		(*remove)(gumboParser_t *gumboParser);    
};

#endif
