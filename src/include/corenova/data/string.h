#ifndef __string_H__
#define __string_H__

#include <corenova/interface.h>
#include <corenova/data/list.h>
#include <corenova/data/md5.h>

#define STRING_MAXLEN 4096

DEFINE_INTERFACE (String) {

    char *    (*new)           (const char *format, ...);
	char *    (*random)        (uint32_t len);
	char *    (*skipwhite)     (const char *);
	char *    (*rskipwhite)    (const char *);
	char *    (*trim)          (const char *);
	ssize_t   (*join)          (char **to, const char *with);
	char *    (*copy)          (const char *orig);
	boolean_t (*equal)         (const char *str1, const char *str2);
	boolean_t (*equalWild)     (const char *str1, const char *str2);
    boolean_t (*equalWildWith) (char with, const char *str1, const char *str2);
	list_t   *(*tokenize)      (const char *string, const char *delim);
	char *    (*itoa2)         (int64_t value);
	char *    (*utoa2)         (uint64_t value);
    md5_t *   (*md5)           (const char *);
    char *    (*first)         (const char *string, const char *match);
    char *    (*last)          (const char *string, const char *match);
    char *    (*translate)     (const char *string, const char match, const char with);
    uint32_t  (*crc32)         (const char *buf, uint32_t len);
    int (*base64encode) (unsigned char *, int, unsigned char *, int);
    int (*base64decode) (unsigned char *, int, unsigned char *, int);
    
};

/* quite honestly if you don't have asprintf too bad
 * this works on Solaris 5.9 though */
#ifndef HAVE_ASPRINTF
#include <stdarg.h>

inline int vasprintf (char **result, const char *format, va_list args)
{
    char p;
    int ret;

    ret = vsnprintf (&p, 1, format, args);

    if (ret > 0) {

        *result = malloc (ret + 1);
        return vsprintf (*result, format, args);
    }

    *result = NULL;

    return ret;
}

inline int asprintf (char **result, const char *format, ...)
{
    va_list va;
    int ret;

    va_start (va, format);
    ret = vasprintf (result, format, va);
    va_end (va);

    return ret;
}
#endif /* HAVE_ASPRINTF */

#ifndef HAVE_STRCASESTR
#include <ctype.h>
inline char * strcasestr (char *haystack, char *needle)
{
    char *p, *startn = 0, *np = 0;

    for (p = haystack; *p; p++) {
        if (np) {
            if (toupper (*p) == toupper (*np)) {
                if (!*++np)
                    return startn;
            } else
                np = 0;
        } else if (toupper (*p) == toupper (*needle)) {
            np = needle + 1;
            startn = p;
        }
    }
    return 0;
}
#endif

#endif

