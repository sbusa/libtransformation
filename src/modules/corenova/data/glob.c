#include <corenova/source-stub.h>

THIS = {
	.version = "2.0",
	.author  = "Peter K. Lee <saint@corenova.com>",
	.description = "This module allows you to glob for files.",
	.implements = LIST ("Glob"),
	.requires = LIST ("corenova.data.string")
};

#include <corenova/data/glob.h>
#include <corenova/data/string.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <glob.h>   /* for glob() */
#include <stdio.h>

static char *
globNameByInode (const char *name, ino_t inode) {
	char *match = NULL;
	char *globpattern = NULL;

	if ((globpattern = I(String)->new("%s*",name))) {
		glob_t globbuf;
		if ((glob (globpattern,0,0,&globbuf) == 0) && (globbuf.gl_pathc > 0)) {
			struct stat st;
			int32_t i = 0;
			for (i = 0; i < globbuf.gl_pathc; i++) {
				if ((stat (globbuf.gl_pathv[i],&st) == 0) &&
					S_ISREG (st.st_mode)) {
					/* file exists & is a regular file */
					if (st.st_ino == inode) {
						match = globbuf.gl_pathv[i];
						break;
					}
				}
			}
		}
		if (match) match = strdup (match);
		globfree(&globbuf);
		free(globpattern);
	}
	return match;
}

static char *
globNextNewerFile (const char *name, struct stat *current_st) {
	char *match = NULL;
	char *globpattern = NULL;

	if ((globpattern = I(String)->new("%s*",name))) {
		glob_t globbuf;
		if ((glob (globpattern,0,0,&globbuf) == 0) && (globbuf.gl_pathc > 0)) {
			time_t choice = 0;
			struct stat st;
			int32_t i = 0;
			for (i = 0; i < globbuf.gl_pathc; i++) {
				if ((stat (globbuf.gl_pathv[i],&st) == 0) &&
					S_ISREG (st.st_mode)) {
					/* file exists & is a regular file */

					if (current_st) {
						/* compare to passed in reference */
						if (current_st->st_ino == st.st_ino)
							continue;
						/* current time is greater than the file being looked at? */
						if (difftime (current_st->st_mtime,st.st_mtime) > 0)
							continue;
					}
					
					/* this file is newer than current file's modification TIME */
					if (choice) {
						if (difftime (st.st_mtime,choice) < 0) {
							choice = st.st_mtime;
							match  = globbuf.gl_pathv[i];
						}
					} else {
						choice = st.st_mtime;
						match  = globbuf.gl_pathv[i];
					}
				}
			}
		}
		if (match) match = strdup (match);
		globfree(&globbuf);
		if (globpattern)
			free(globpattern);
	}
	return match;
}

IMPLEMENT_INTERFACE (Glob) = {
	.nameByInode   = globNameByInode,
	.nextNewerFile = globNextNewerFile
};
