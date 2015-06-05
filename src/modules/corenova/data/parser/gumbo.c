#include <corenova/source-stub.h>
#include <corenova/data/parser/gumboparser.h>

THIS = {
    .version = "0.1",
    .author = "Suresh Kumar/Hash Yuan",
    .description = "This module provides a set of gumbo html5 parser specific operations.",
    .implements = LIST("Gumbo"),
    .requires = LIST("corenova.data.array", "corenova.data.string")
};


#include <corenova/data/array.h>
#include <corenova/data/string.h>
#include <string.h>


/*//////// MODULE CODE //////////////////////////////////////////*/

//utility functions

//removing the TAG is achieved, by shifting the remaining string to the TAG position
static void _removeTag(gumboParser_t *gumboParser,int pos, int len)
{	
	printf("inside removeTag bufflen (%d) pos %d len %d\n",gumboParser->buffer_lenth,pos,len);
	int shifting_length = gumboParser->buffer_lenth - (pos+len);
	printf(" shift len %d \n",shifting_length);
	
	strncpy((gumboParser->buffer+pos),(gumboParser->buffer+pos+len),shifting_length);
	*(gumboParser->buffer+pos+shifting_length)='\0';
	printf("new buffer len %d",(int) strlen(gumboParser->buffer));
	gumboParser->buffer_lenth=strlen((gumboParser->buffer));	
}

static int  _getBufferLength(gumboParser_t *gumboParser)
{
	return (gumboParser->buffer_lenth);
}


//Interface functions

static gumboParser_t * newGumboParser(char *data, int size)
{

	GumboOutput *output  = gumbo_parse_with_options(&kGumboDefaultOptions, data, size);
	if (output) {
		gumboParser_t *gumboParser = (gumboParser_t *)calloc (1,sizeof (gumboParser_t));
		if (gumboParser)
		{
			gumboParser->output = output;
			gumboParser->buffer_lenth = size;
			gumboParser->buffer = data;
			
			//create an Array 
			gumboParser->match_refs = I(Array)->new();
			MUTEX_SETUP (gumboParser->lock);
		
			return gumboParser;
		}
	} else {
			DEBUGP(DDEBUG, "gumboParser", "can't parse the data");	
	}
	
	return NULL;
}


static void destroyGumboParser(gumboParser_t *gumboParser)
{
	DEBUGP (DDEBUG,"destroyGumboParser","inside destroygumboparser");
	if (gumboParser)
	{
		gumbo_destroy_output(&kGumboDefaultOptions, gumboParser->output);
		if(gumboParser->buffer)
		{			
			gumboParser->buffer = NULL;
		}
		gumboParser->buffer_lenth=0;
		//Todo : array to be destroyed.

        if (gumboParser->match_refs) {
            I (Array)->destroy (&gumboParser->match_refs,free);
        }
        
		MUTEX_CLEANUP (gumboParser->lock);
	}
}


static char* _getBuffer(gumboParser_t *gumboParser)
{
	return (gumboParser->buffer);
}



/*
<li class="g"><h3 class="r"><a href="/url?q=http://gunsmagazine.com/&amp;sa=U&amp;ei=Y1MFU8eWHI_yoASln4C4DQ&amp;ved=0CGEQFjAN&amp;usg=AFQjCNF_1WudljQUtpi41o_2cBptfRD6Eg"><b>Guns</b> Magazine</a></h3><div class="s"><div class="kv" style="margin-bottom:2px"><cite><b>guns</b>magazine.com/</cite><div class="am-dwn-arw-container">&#8206;<div onclick="google.sham(this);" aria-expanded="false" aria-haspopup="true" tabindex="0" data-ved="0CGIQ7B0wDQ" style="display:inline"><span class="am-dwn-arw"></span></div><div class="am-dropdown-menu" role="menu" tabindex="-1" style="display:none"><ul><li class="am-dropdown-menu-item"><a class="am-dropdown-menu-item-text" href="/url?q=https://webcache.googleusercontent.com/search%3Fq%3Dcache:bex0aogegV0J:http://gunsmagazine.com/%252Bguns%26hl%3Den%26ct%3Dclnk&amp;sa=U&amp;ei=Y1MFU8eWHI_yoASln4C4DQ&amp;ved=0CGQQIDAN&amp;usg=AFQjCNEhsdusWFRpxMO2Ink6mLzGloEodQ">Cached</a></li><li class="am-dropdown-menu-item"><a class="am-dropdown-menu-item-text" href="/search?ie=UTF-8&amp;q=related:gunsmagazine.com/+guns&amp;tbo=1&amp;sa=X&amp;ei=Y1MFU8eWHI_yoASln4C4DQ&amp;ved=0CGUQHzAN">Similar</a></li></ul></div></div></div><span class="st"><b>Gun</b> rights advocates were quick to spot the ignorance that defined <b>...</b> he called br>
ghost <b>guns</b>,his hysterical legislative response to plastic printing technology&nbsp;<b>...</b></span><br></div></li>
*/

//Eg calling method.
//GumboParserTag tagName = GUMBO_TAG_LI;
//char *attr = "class";
//char *value = "g";  

static void
matchTag(gumboParser_t *gumboParser, GumboNode *node, GumboTag tagName, char *attribute, char *value)
{
	if (gumboParser && node && node->type == GUMBO_NODE_ELEMENT && tagName) {

		if (node->v.element.tag == tagName) {
            boolean_t foundMatch = FALSE;
            

            if (attribute) {    /* specified attribute to match! */
                GumboAttribute *attr = NULL;
                if ((attr = gumbo_get_attribute(&node->v.element.attributes, attribute))) {

                    if (value) { /* specified value to match! */
			char *regex = strchr(value, '*');

			if (regex) {
				if (I (String)->equalWild(attr->value, value))
				foundMatch = TRUE;
			} else {
                        	int len = strlen(value);
				char *match;
				
				/* "g", "g ", " g", " g " */
				if ((match = strstr(attr->value, value))) {
					if ((match == attr->value || *(match - 1) == ' ') &&
						(*(match + len)  == '\0' ||  *(match + len) == ' '))
					foundMatch = TRUE;
				}
			}
                    } else {
                        foundMatch = TRUE;
                    }
                }
            } else {
                foundMatch = TRUE;
            }

            if (foundMatch) {
                tag_t *tag = calloc (1, sizeof (tag_t));
                if (tag) {
					tag->node = node;
					tag->start = gumboParser->buffer + node->v.element.start_pos.offset;
					tag->length = node->v.element.end_pos.offset + node->v.element.original_end_tag.length - node->v.element.start_pos.offset;

                    /* we embed the match_refs array here so that we can call gumboParser without any array at start */
                    if (!gumboParser->match_refs) {
                        gumboParser->match_refs = I (Array)->new ();
                    }

                    if (gumboParser->match_refs) 
                        I(Array)->add(gumboParser->match_refs, tag);
    	    	
					DEBUGP(DDEBUG, "matchTag", "position: %p, length: %d", tag->start, tag->length);
                }
            }
        }

        // default fallback is to look at the node to see if there are children and recurse to it
        GumboVector *children = &node->v.element.children;
        if (children && children->length) {
            int i;
            for (i = 0; i < children->length; ++i)
            {
                GumboNode *element = children->data[i];
				
                if (element->type == GUMBO_NODE_ELEMENT) {
                    matchTag(gumboParser, element, tagName, attribute, value);
                }
            }
        }
	}
}

static char *
getAttributeValueFromNode(GumboNode *node, const char *attribute) {
    char *val = NULL;
    if (node && node->type == GUMBO_NODE_ELEMENT && attribute) {
        GumboAttribute *attr = gumbo_get_attribute(&node->v.element.attributes, attribute);
        if (attr)
            val = attr->value;
    }
    return val;
} 

// This function is used for removing the matched tags,by match function call.
static void removeTags(gumboParser_t *gumboParser)
{
#if 0
	 int tmpcnt=I(Array)->count(gumboParser->match_refs);
	   printf("Array count is %d\n",tmpcnt);
	   int i=0;
	   for(i=tmpcnt-1;i>=0;i--)
	   {
		   tag_t *tag = I(Array)->get(gumboParser->match_refs,i);
		   printf("%d = %d\n",tag->sart, tag->length);		
		   _removeTag(gumboParser,tag->start, tag->length);		   
		   free(tag);
	   }
#endif
}


IMPLEMENT_INTERFACE(Gumbo) = {
    .new = newGumboParser,
    .toString = _getBuffer,
    .destroy = destroyGumboParser,
    .match = matchTag,
    .remove = removeTags,
    .getAttrValue = getAttributeValueFromNode
};
