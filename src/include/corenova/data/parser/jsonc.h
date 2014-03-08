#ifndef __jsonc_parser_H__
#define __jsonc_parser_H__

#include <corenova/interface.h>
#include <corenova/data/array.h>


typedef struct 
{	
	int  buffer_lenth;			//Length of the input buffer
	char *buffer;			   //pointer to the input buffer
	array_t *match_refs;	   // stores the positions of the matches 
    MUTEX_TYPE lock;           /* makes operations thread-safe */    
} jsonStringParser_t;


typedef struct 
{
	char *start;
	uint32_t length;
	char *name;	
} json_tag_t;

#define TAG_BUF 256

/*//////// MODULE DECLARATION //////////////////////////////////////////*/

DEFINE_INTERFACE (jsonc)
{
	jsonStringParser_t*  (*new)     (char *, int length);	
	void      		(*destroy) (jsonStringParser_t *); 
    int  	   		(*match)(jsonStringParser_t *jsonStringParser, char *tagName, char *attribute, char *value);
    void      		(*remove) (jsonStringParser_t *); 
    char *			(*toString)(jsonStringParser_t *);       
};

#endif
