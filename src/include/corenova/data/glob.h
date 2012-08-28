#ifndef __corenova_data_glob_H__
#define __corenova_data_glob_H__

#include <corenova/interface.h>

#include <sys/stat.h>			/* for ino_t */
#include <time.h>				/* for time_t */

DEFINE_INTERFACE (Glob) {
	char *(*nameByInode)   (const char *name, ino_t);
	char *(*nextNewerFile) (const char *name, struct stat *);
};

#endif
