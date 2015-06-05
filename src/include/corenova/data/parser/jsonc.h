#ifndef __jsonc_parser_H__
#define __jsonc_parser_H__

#include <corenova/interface.h>
#include <corenova/data/array.h>

#include <json-c/json.h>

enum {
	JSON_OBJECT = 1,
	JSON_ARRAY,
	JSON_BOOLEAN,
	JSON_INT,
	JSON_INT64,
	JSON_DOUBLE,
	JSON_DOUBLE_S,
	JSON_STRING,
	JSON_STRING_LEN
} types;

/*//////// MODULE DECLARATION //////////////////////////////////////////*/

DEFINE_INTERFACE (jsonc)
{
    json_object *(*newObject) (int type);
    void (*destroyObject) (json_object *);
    void (*addObject) (json_object *, int type, char *, void *);
    void (*addArray) (json_object *, int type, void *);
    char *(*toString) (json_object *);
};

#endif
