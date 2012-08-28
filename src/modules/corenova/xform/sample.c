#include <corenova/source-stub.h>

THIS = {
	.version     = "1.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "This is a sample module that transforms a file into md5 checksum, or spits out contents line by line",
    .implements  = LIST ("Transformation"),
	.requires    = LIST ("corenova.data.string",
                         "corenova.data.file",
                         "corenova.data.md5"),
    .transforms  = LIST ("data:file:*->data:string:md5",
                         "data:file:*->data:string:line")
};

#include <corenova/sys/transform.h>
#include <corenova/data/string.h>
#include <corenova/data/file.h>
#include <corenova/data/md5.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

static int
xDataFileToDataStringMD5 (transform_ctx_t *ctx, void *in, void * out) {
    
}

static void freeDataStringMD5 ()

T (DataFileToDataStringMD5)->free ()

IMPLEMENT_INTERFACE (Transformation) = {
    .new     = newTransformation,
    .execute = executeTransformation,
    
};

IMPLEMENT_OBJECT ("data:string:md5") = {
    .new = blah,
    .destroy = blah,
    .serialize = NULL,
    .deserialize = NULL
};
