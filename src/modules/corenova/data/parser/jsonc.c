#include <corenova/source-stub.h>
#include <corenova/data/parser/jsonc.h>
#include <corenova/data/array.h>
#include <corenova/data/string.h>
#include <string.h>


THIS = {
    .version = "0.1",
    .author = "Suresh Kumar ",
    .description = "This module provides  api to remove the URL from the google serach response json file.",
    .implements = LIST("jsonc"),
    .requires = LIST("corenova.data.array")
};


/*//////// MODULE CODE //////////////////////////////////////////*/
#if 0
//removing the TAG is achieved, by shifting the remaining string to the TAG position
static void _removeTag(jsonStringParser_t *jsonStringParser,int pos, int len)
{

	//printf("inside removeTag bufflen (%d) pos %d len %d\n",jsonStringParser->buffer_lenth,pos,len);
	int shifting_length = jsonStringParser->buffer_lenth - (pos+len);
	//printf(" shift len %d \n",shifting_length);
	
	strncpy((jsonStringParser->buffer+pos),(jsonStringParser->buffer+pos+len),shifting_length);
	*(jsonStringParser->buffer+pos+shifting_length)='\0';
	//printf("new buffer len %d",strlen(jsonStringParser->buffer));
	jsonStringParser->buffer_lenth=strlen((jsonStringParser->buffer));	

}
#endif

static int matchtag(jsonStringParser_t *jsonStringParser, char *tagName, char *attribute, char *value)
{
	char tag_start[TAG_BUF];
	char tag_end[TAG_BUF];
	int match = 0;
	char *ptr, *bufend;
	int tag_start_len, tag_end_len;

	snprintf(tag_start, TAG_BUF, "\\\\x3c%s %s\\\\x3d\\\\x22%s", tagName, attribute, value);
	tag_start_len = strlen(tag_start);

	snprintf(tag_end, TAG_BUF, "\\\\x3c\\/%s\\\\x3e", tagName);
	tag_end_len = strlen(tag_end);

	ptr = jsonStringParser->buffer;
	bufend = ptr + jsonStringParser->buffer_lenth;
	while(ptr < bufend)
	{
		char *t1 = strstr(ptr, tag_start);
		if(t1)
		{
			t1 += tag_start_len;
			char *t2 = strstr(t1, "\\\\x22\\\\x3e");
			if (t2) {
				*t2 ='\0';
				
				if (*t1 == ' ' || strlen(t1) == 0) {
					*t2 = '\\';
					t2 += sizeof("\\\\x22\\\\x3e") - 1;
					
					char *t3 = strstr(t2, tag_end);
					if (t3) {
						json_tag_t *tag = (json_tag_t *)calloc(1, sizeof(json_tag_t));
						if (tag) {
							tag->start = t1 - tag_start_len;
							tag->length = t3 - tag->start + tag_end_len;

							I(Array)->add(jsonStringParser->match_refs, tag);
							match ++;
						}
						
						ptr = t3 + tag_end_len;
						continue;
					} else {
						DEBUGP(DDEBUG, "matchtag", "cant find tag_end %s at %s\n", tag_end, t2);
					}
				} else {
					DEBUGP(DDEBUG, "matchtag", "cant find the value %s at %s\n", value, t1);	
				}

				ptr = t2 + sizeof("\\\\x22\\\\x3e") - 1;
				*t2 = '\\';
			} else {
				ptr = t1 + tag_start_len;
				printf("string not found \\x22\\x3e");
			}
		} else {
			DEBUGP(DDEBUG, "matchtag", "string not found %s\n", tag_start);
			break;
		}
	}
	
	return match;
}



//Interface functions

static jsonStringParser_t * newJsonStringParser(char *data, int len)
{
	DEBUGP(DDEBUG, "JsonStringParser", "newJsonStringParser");
	jsonStringParser_t *jsonStringParser = (jsonStringParser_t *)calloc (1,sizeof (jsonStringParser_t));
	if (jsonStringParser)
	{
		jsonStringParser->buffer_lenth = len;
		jsonStringParser->buffer = data;
		jsonStringParser->match_refs = I(Array)->new();
		MUTEX_SETUP (jsonStringParser->lock);
	}   
	return jsonStringParser;
}


static void destroyJsonStringParser(jsonStringParser_t *jsonStringParser)
{
	if (jsonStringParser)
	{
		jsonStringParser->buffer = NULL;
		jsonStringParser->buffer_lenth=0;
		I(Array)->destroy(&jsonStringParser->match_refs, NULL);
		MUTEX_CLEANUP (jsonStringParser->lock);
		
		free(jsonStringParser);
	}
}

static void removeJsonUrl(jsonStringParser_t *jsonStringParser)
{
#if 0
	 int tmpcnt=I(Array)->count(jsonStringParser->match_refs);
	   //printf("Array count is %d\n",tmpcnt);
	 int i=0;
	 for(i=tmpcnt-1;i>=0;i--)
	 {
		 posReferences_t *t=I(Array)->get(jsonStringParser->match_refs,i);
		 //printf("%d = %d\n",t->offset,t->length);		
		 _removeTag(jsonStringParser,t->offset,t->length);		   
		 free(t);
	 }	
#endif
}

static char* _getBuffer(jsonStringParser_t *jsonStringParser)
{
	return (jsonStringParser->buffer);
}


IMPLEMENT_INTERFACE(jsonc) = {
    .new = newJsonStringParser,
    .toString = _getBuffer,
    .destroy = destroyJsonStringParser,
    .match = matchtag,
    .remove = removeJsonUrl
};
