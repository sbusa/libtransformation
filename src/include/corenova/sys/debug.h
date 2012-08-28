#ifndef __corenova_sys_debug_H__
#define __corenova_sys_debug_H__

#include <corenova/interface.h>

DEFINE_INTERFACE (Debug) {
    void (*logDir) (const char *dir);
};

#endif
