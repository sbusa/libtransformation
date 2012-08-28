#include <corenova/source-stub.h>

THIS = {
    .version = "1.0",
    .author = "Alex Burkoff <alex.burkoff@gmail.com>",
    .description = "Hello, World program",
    .implements = LIST("Transformation"),
    .requires = LIST("corenova.sys.transform"),
    .transforms = LIST("* -> hello",
    "* -> world",
    "* -> sleep"
    )
};

#include <corenova/sys/transform.h>
#include <corenova/data/string.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

TRANSFORM_EXEC(any2hello) {

    DEBUGP(DDEBUG, "feeder2hello", "hello");

    transform_object_t *obj = I(TransformObject)->new("hello", NULL);
    
    return obj;

}

TRANSFORM_EXEC(any2world) {

    DEBUGP(DDEBUG, "hello2world", "world");
    
    transform_object_t *obj = I(TransformObject)->new("world", NULL);

    return obj;

}

TRANSFORM_EXEC(any2sleep) {
    
    int sleep_time = atoi (I (Parameters)->getValue (xform->blueprint,"sleep_time"));
    
    if(sleep_time <= 0) {
        
        sleep_time = 1;
        
    }

    DEBUGP(DDEBUG, "any2sleep", "sleeping %u seconds", sleep_time);
    
    sleep(sleep_time);
    
    transform_object_t *obj = I(TransformObject)->new("sleep", NULL);
    
    return obj;

}


TRANSFORM_NEW(newTransformation) {

    TRANSFORM("*", "hello", any2hello);
    TRANSFORM("*", "world", any2world);
    TRANSFORM("*", "sleep", any2sleep);

}
TRANSFORM_NEW_FINALIZE;



TRANSFORM_DESTROY(destroyTransformation) {



}
TRANSFORM_DESTROY_FINALIZE;

IMPLEMENT_INTERFACE(Transformation) = {
    .new = newTransformation,
    .execute = NULL,
    .destroy = destroyTransformation,
    .free = NULL

};


