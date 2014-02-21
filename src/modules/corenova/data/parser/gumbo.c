#include <corenova/source-stub.h>
#include <corenova/data/parser/gumboparser.h>

THIS = {
    .version = "0.1",
    .author = "Suresh Kumar ",
    .description = "This module provides a set of gumbo html5 parser specific operations.",
    .implements = LIST("Gumbo"),
    .requires = LIST("corenova.data.array")
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
	printf("new buffer len %d",strlen(gumboParser->buffer));
	gumboParser->buffer_lenth=strlen((gumboParser->buffer));	
}

// utility function to update the array with identified postions of a match.
static void _updateMatchResult(gumboParser_t *gumboParser,int position,int length)
{
	tagReferences_t *t=calloc(1,sizeof(tagReferences_t));
	t->offset= position;
	t->length= length;
	I(Array)->add(gumboParser->match_refs, t);	
}

// This function travel the gumboParser node tree and identify the match (A_TAG, href)
// and update the position,length in to the arrary. Later this array will be used by remove function to remove the tag.
// this function is a recursive function.
static int _findattribute(gumboParser_t *gumboParser, const char *link)
{
	GumboNode *node=gumboParser->currentnode;
	if (node->type != GUMBO_NODE_ELEMENT)
	{
		return 0; //failure case or complete traversal is done
	}
	GumboAttribute* attptr;
	if (node->v.element.tag == GUMBO_TAG_A && (attptr = gumbo_get_attribute(&node->v.element.attributes, "href")))
	{
		if(strstr(attptr->value,link))
		{		 
			printf("findattribute found\n");
			
			int position= node->v.element.start_pos.offset;		    
		    int endoffset= node->v.element.end_pos.offset + node->v.element.original_end_tag.length;		    
			int length = (endoffset - position);
			printf("position is (%d) length is (%d)\n",position,length);
		   _updateMatchResult(gumboParser,position,length);		
		}
	  }
	  else
	  {
		  GumboVector* children = &node->v.element.children;
		  int i=0;
		  for (i = 0; i < children->length; ++i)
		  {
			  gumboParser->currentnode=children->data[i];
			  _findattribute(gumboParser,link);
		  }
	  }
	  return 1;
}


static int  _getBufferLength(gumboParser_t *gumboParser)
{
	return (gumboParser->buffer_lenth);
}


//Interface functions

static gumboParser_t * newGumboParser(char *data, int len)
{
	DEBUGP(DDEBUG, "gumboParser", "newGumboParser");
	//printf("inside newGumboParser\n");
	gumboParser_t *gumboParser = (gumboParser_t *)calloc (1,sizeof (gumboParser_t));
	if (gumboParser)
	{
        // Build gumbo tree out of the data supplied
		gumboParser->buffer_lenth=len;
		gumboParser->buffer=(char *)calloc(1,sizeof(char));
		
		if(gumboParser->buffer== NULL)
		{			
			return NULL;
		}
		//printf("%s\n",data);
		gumboParser->buffer = data;
				
		//printf("buffer address %u = %u\n buffer pts to add value (%s) = (%s)\n", data, gumboParser->buffer, data, (gumboParser->buffer));
			
		//create a gumbo parser tree with the given buffer and length input.
		gumboParser->output=gumbo_parse_with_options(&kGumboDefaultOptions,gumboParser->buffer,gumboParser->buffer_lenth);
		
		//assign the root node in to the current node
		gumboParser->currentnode=gumboParser->output->root;
		
		//create a Array 
		gumboParser->match_refs= I(Array)->new();
		MUTEX_SETUP (gumboParser->lock);
	}   
	return gumboParser;
}


static void destroyGumboParser(gumboParser_t *gumboParser)
{
	printf("inside destroygumboparser\n");
	if (gumboParser)
	{
		gumbo_destroy_output(&kGumboDefaultOptions, gumboParser->output);
		if(gumboParser->buffer)
		{			
			gumboParser->buffer = NULL;
		}
		gumboParser->buffer_lenth=0;
		//Todo : array to be destroyed.
		MUTEX_CLEANUP (gumboParser->lock);
	}
}


static char* _getBuffer(gumboParser_t *gumboParser)
{
	return (gumboParser->buffer);
}



//Search the TAG Name, and given attribute value text of the tag name and populates the offset,length in the array. 
//Currently implemented only A TAG, and href attribute value. Eg:  In <A> tag,href="google.com"
//input :  GumboParser structure,  TAG Name (ENUM), attribute, attribute value

//Eg calling method.
//GumboParserTag mytag=GUMBOPARSER_TAG_A;
//char *at="href";
//char *link="google.com";  
//I(GumboParser)->match(myparser,mytag,at,link);

static void matchTag(gumboParser_t *gumboParser,GumboParserTag tagName, char *attribute, char *text)
{	
	int pos=0,len=0;	
	printf("_removeLinks link name is %s, tagname %d ATAG %d\n",text,tagName,GUMBO_TAG_A);
	if((tagName==GUMBO_TAG_A) && (!strncmp(attribute,"href",4)))
	{
		printf("calling findattribute fun\n");
		_findattribute(gumboParser,text);
	}	
}

// This function is used for removing the matched tags,by match function call.
static void removeTags(gumboParser_t *gumboParser)
{
	 int tmpcnt=I(Array)->count(gumboParser->match_refs);
	   printf("Array count is %d\n",tmpcnt);
	   int i=0;
	   for(i=tmpcnt-1;i>=0;i--)
	   {
		   tagReferences_t *t=I(Array)->get(gumboParser->match_refs,i);
		   printf("%d = %d\n",t->offset,t->length);		
		   _removeTag(gumboParser,t->offset,t->length);		   
		   free(t);
	   }
	   
}



IMPLEMENT_INTERFACE(Gumbo) = {
    .new = newGumboParser,
    .toString = _getBuffer,
    .destroy = destroyGumboParser,
    .match = matchTag,
    .remove = removeTags
};
