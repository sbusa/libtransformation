/*
  the following should be included at the top of every source file that
  aims to serve as a corenova-compatible module.

  The purpose is to override predefined C macros so that proper LTDL-style
  symbols can be created.

  This prevents symbol conflicts when all symbols are PRELOADED into a
  static binary.
*/
#include <corenova/module.h>
#include <corenova/interface.h>

#include <corenova/debug.h>		/* for debugging */

#undef  DEBUG_SOURCE
#define DEBUG_SOURCE LT_SYMBOL(this).name

#ifdef MODULE

# undef  LT_SYMBOL
# define LT_SYMBOL(X) LT_SYMBOL2(MODULE,X)

#else  /* this is a program */

# undef  LT_SYMBOL
# define LT_SYMBOL(X) X

static void CONSTRUCTOR __init__ (void) { void nova_init (void); nova_init (); }
static void DESTRUCTOR  __exit__ (void) { void nova_exit (void); nova_exit (); }

#endif

extern boolean_t SystemExit;
extern boolean_t NovaCache;

extern interface_t *nova_interface_lookup (module_t *module, const char *name);

static inline interface_t *
findInterface (module_t *module, const char *name) {

    if (NovaCache)
        return nova_interface_lookup (module, name);
    
    if (module && name) {

        /* search internal interface list */
        if (module->implements) {

            int32_t interfaceIndex = 0;
            while (module->implements[interfaceIndex]) {

                const char *interfaceName = module->implements[interfaceIndex];
                if (!strncmp (interfaceName,name,INTERFACE_NAME_MAXLEN)) {

                    return module->interfaces[interfaceIndex];
                }
                interfaceIndex++;
            }
            /* not in this module, search dependent modules */
            if (module->requires) {

                int32_t moduleIndex = 0;
                while (module->requires[moduleIndex]) {

                    interface_t *found = findInterface (module->modules[moduleIndex], name);
                    if (found)
                        return found;
                    moduleIndex++;
                }
            }
        }
    }
    //      if (module == &LT_SYMBOL (this)) {

    /* couldn't find in the CALLING module!  This is BAD NEWS! */
    /* how do we preempt a CERTAIN segfault here? */
    //      }

    return NULL;
}

static inline module_t *
findModule (module_t *this, const char *name) {
	if (this) {
		if (this->requires) {
			int32_t moduleIndex = 0;
			while (this->requires[moduleIndex]) {
				const char *moduleName = this->requires[moduleIndex];
				if (!strncmp (moduleName,name,MODULE_NAME_MAXLEN)) {
					return this->modules[moduleIndex];
				}
				moduleIndex++;
			}
		}
	}
	return NULL;
}
