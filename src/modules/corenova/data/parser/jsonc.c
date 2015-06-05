#include <corenova/source-stub.h>
#include <corenova/data/parser/jsonc.h>
#include <corenova/data/array.h>
#include <corenova/data/string.h>
#include <string.h>


THIS = {
    .version = "0.1",
    .author = "Hash Yuan",
    .description = "A interface of json-c",
    .implements = LIST("jsonc"),
    .requires = LIST("corenova.data.array")
};


/*//////// MODULE CODE //////////////////////////////////////////*/

json_object *newObject (int type)
{
	if (type == JSON_OBJECT)
		return json_object_new_object();
	else if (type == JSON_ARRAY)
		return json_object_new_array();

	return NULL;
}


void destroyObject(json_object *json)
{
	json_object_put(json);
}

void addObject(json_object *json, int type, char *key, void *value)
{
	if (json) {
		json_object *object = NULL;

		switch (type) {
			case JSON_INT:
			object = json_object_new_int((int)value);
			break;

			case JSON_STRING:
			object = json_object_new_string((char *)value);
			break;
			
			case JSON_BOOLEAN:
			object = json_object_new_boolean((int)value);
			break;
			
			case JSON_ARRAY:
			object = (json_object *)value;
			break;
		}
		
		if (object) json_object_object_add(json, key, object);
	}
}

void addArray(json_object *json, int type, void *value)
{
	if (json) {
		json_object *object = NULL;

		switch (type) {
			case JSON_INT:
			object = json_object_new_int((int)value);
			break;
                        
			case JSON_STRING:
			object = json_object_new_string((char *)value);
			break;

			case JSON_BOOLEAN:
			object = json_object_new_boolean((int)value);
			break; 
		}

		if (object) json_object_array_add(json, object);	
	}
}

char *toString(json_object *json)
{
	return json_object_to_json_string(json);
}

IMPLEMENT_INTERFACE(jsonc) = {
    .newObject = newObject,
    .destroyObject = destroyObject,
    .addObject = addObject,
    .addArray = addArray,
    .toString = toString,
};
