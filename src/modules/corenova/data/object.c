#include <corenova/source-stub.h>

THIS = {
	.version     = "3.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "Encapsulates various data objects.",
	.implements  = LIST ("BinaryObject","TextObject","Transformation"),
    .requires    = LIST ("corenova.sys.transform"),
    .transforms  = LIST ("data:object::* -> data:object:format::*")
};

#include <corenova/data/object.h>
#include <corenova/sys/transform.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

static binary_t *
_newBinaryObject (void *data, uint32_t size, const char *format) {
	binary_t *bin = (binary_t *)calloc (1,sizeof (binary_t));
	if (bin) {
        if (format) {
            bin->format = strdup (format);
            bin->size = size;
            bin->data = data;
        } else {
            DEBUGP (DWARN,"_newBinaryObject","attempting to create object without format specifier!");
            free (bin);
            bin = NULL;
        }
	}
	return bin;
}

static binary_t *
_cloneBinaryObject (binary_t *bobj) {
    if (bobj) {
        void *copy = NULL;
        DEBUGP (DDEBUG,"_cloneBinaryObject","trying to clone: %s at %p of size %u",bobj->format?bobj->format:"unknown",bobj->data,bobj->size);
        if (bobj->size) {
            copy = malloc (bobj->size);
            if (copy) {
                memcpy (copy,bobj->data,bobj->size);
            }
        } 
        return _newBinaryObject (copy,bobj->size,bobj->format);
    }
    return NULL;
}

static void
_destroyBinaryObject (binary_t **binPtr) {
	if (binPtr) {
		binary_t *bin = *binPtr;
		if (bin) {
            DEBUGP (DDEBUG,"_destroyBinaryObject","destroying %p",bin);
            free (bin->format);
			free (bin->data);
			free (bin);
			*binPtr = NULL;
		}
	}
}

IMPLEMENT_INTERFACE (BinaryObject) = {
	.new     = _newBinaryObject,
    .clone   = _cloneBinaryObject,
	.destroy = _destroyBinaryObject
};

static text_t *
_newTextObject (char *data, uint32_t size, const char *format) {
	text_t *txt = (text_t *)calloc (1,sizeof (text_t));
	if (txt) {
        if (format) {
            txt->format = strdup (format);
            txt->data = data;
            txt->size = size;
        } else {
            DEBUGP (DWARN,"_newTextObject","attempting to create object without format specifier!");
            free (txt);
            txt = NULL;
        }
	}
	return txt;
}

static text_t *
_cloneTextObject (text_t *tobj) {
    if (tobj) {
        void *copy = NULL;
        if (tobj->size) {
            copy = malloc (tobj->size);
            if (copy) {
                memcpy (copy,tobj->data,tobj->size);
            }
        } 
        return _newTextObject (copy,tobj->size,tobj->format);
    }
    return NULL;
}

static void
_destroyTextObject (text_t **txtPtr) {
	if (txtPtr) {
		text_t *txt = *txtPtr;
		if (txt) {
			free (txt->format);
            free (txt->data);
			free (txt);
			*txtPtr = NULL;
		}
	}
}

IMPLEMENT_INTERFACE (TextObject) = {
	.new     = _newTextObject,
    .clone   = _cloneTextObject,
	.destroy = _destroyTextObject
};

/*//////// Transformation Interface Implementation //////////////////////////////////////////*/

TRANSFORM_EXEC (obj2format) {
    /*
     * conversion from data:object into data:object:format::*
     * 
     * does NOT depend on XFORM->INSTANCE
     */
    if (I (String)->equal (in->format,"data:object::binary")) {
        binary_t *binary = (binary_t *)in->data;
        char *format = I (String)->copy ("data:object:format::");
        if (I (String)->join (&format,binary->format)) {
            transform_object_t *obj = I (TransformObject)->new (format,format);
            if (obj) {
                return obj;
            }
        }
        free (format);
    }
    else if (I (String)->equal (in->format,"data:object::text")) {
        text_t *text = (text_t *)in->data;
        char *format = I (String)->copy ("data:object:format::");
        if (I (String)->join (&format,text->format)) {
            transform_object_t *obj = I (TransformObject)->new (format,format);
            if (obj) {
                return obj;
            }
        }
        free (format);
    }
    return NULL;
}

TRANSFORM_NEW (newObjectTransformation) {

    TRANSFORM ("data:object::*","data:object:format::*",obj2format);
    
} TRANSFORM_NEW_FINALIZE;

TRANSFORM_DESTROY (destroyObjectTransformation) {

} TRANSFORM_DESTROY_FINALIZE;


IMPLEMENT_INTERFACE (Transformation) = {
	.new       = newObjectTransformation,
	.destroy   = destroyObjectTransformation,
	.execute   = NULL,
	.free      = NULL
};
