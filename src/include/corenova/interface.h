#ifndef __interface_H__
#define __interface_H__

#include <corenova/core.h>

/*//////// MACROS for interfaces ///////////////////////////////*/

#define INTERFACE_NAME_MAXLEN 100
/*
  Call this in *.h file that describes the NAMEd interface for other
  modules to lookup when making calls to modules that IMPLEMENTS the NAMEd
  interface.
*/
#define DEFINE_INTERFACE(NAME) \
	typedef I_STRUCT (NAME) I_TYPE (NAME); \
	I_STRUCT (NAME)

/*
  Call this in *.c source files that implement the NAMEd interface
*/
#define IMPLEMENT_INTERFACE(NAME) I_STRUCT (NAME) NAME

/*
  generic macro to access the interface of a specified module
*/
#define I_ACCESS(MODULE,NAME) ((I_TYPE (NAME) *) findInterface (MODULE, STR (NAME)))

/*
  wrapper around I_ACCESS to find matching NAME interface w/in the
  "current" module's interface table
*/
#define I(NAME) I_ACCESS (&LT_SYMBOL (this), NAME)
//#define I(NAME) ((I_TYPE (NAME) *) INTERFACE (&LT_SYMBOL (this), STR (NAME)))

/*
  wrapper around I_ACCESS to find "MODULE_STR" in the current module, and
  access interface for the specified module.
*/
#define I_DIRECT(MODULE_STR,NAME) I_ACCESS (findModule (&LT_SYMBOL (this), MODULE_STR),NAME)

/*
  Other interface macros, should have seldom use.
*/
#define I_TYPE(NAME) CONC (NAME,_interface_t)

#define I_STRUCT(NAME) struct CONC (NAME,_interface)

#define I_EXISTS(NAME) (I (NAME) != NULL)

#endif
