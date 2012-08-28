#ifndef __macros_H__
#define __macros_H__

#ifdef __cplusplus
#define BEGIN_C_DECLS extern C {
#define END_C_DECLS }
#else
#define BEGIN_C_DECLS 
#define END_C_DECLS
#endif

#ifdef __STDC__
#define STR(x)      #x
#define CONC(x,y)   x##y
#define CONC3(x,y,z) x##y##z
#else
#define STR(x)      "x"
#define CONC(x,y)   x/**/y
#define CONC3(x,y,z) x/**/y/**/z
#endif

#define STR_EQ(X,Y,Z) !strncmp(X,Y,Z?Z:strlen(Y))
#define STRC_EQ(X,Y,Z) !strncasecmp(X,Y,Z?Z:strlen(Y))

#define PARAM(key,type)	{ key, STR(type) },

#endif /* __macros_H__ */

