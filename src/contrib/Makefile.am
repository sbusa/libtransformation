###
# build ubiqx convenience library

# figure out how to make this work later...

noinst_LTLIBRARIES = libubiqx.la
libubiqx_la_SOURCES =							\
	ubiqx/library/ubi_AVLtree.c					\
	ubiqx/library/ubi_BinTree.c					\
	ubiqx/library/ubi_SplayTree.c				\
	ubiqx/library/ubi_Cache.c					\
	ubiqx/library/ubi_dLinkList.c				\
	ubiqx/library/ubi_sLinkList.c				\
	ubiqx/library/ubi_SparseArray.c
libubiqx_la_INCLUDE = -Iubiqx/library
libubiqx_la_LDFLAGS = -no-undefined -avoid-version -module
# extra stuff to include in dist
EXTRA_libubiqx_la_SOURCES =						\
	ubiqx/library/sys_include.h					\
	ubiqx/library/ubi_AVLtree.h					\
	ubiqx/library/ubi_BinTree.h					\
	ubiqx/library/ubi_Cache.h					\
	ubiqx/library/ubi_SparseArray.h				\
	ubiqx/library/ubi_SplayTree.h				\
	ubiqx/library/ubi_dLinkList.h				\
	ubiqx/library/ubi_sLinkList.h

# below was experiemental...

# SUBDIRS=tcc-0.9.26
# noinst_LTLIBRARIES += libtcc.la
# libtcc_la_LDADD = tcc-0.9.26/libtcc.a
# libtcc_la_INCLUDE = -Itcc-0.9.26/library
# libtcc_la_LDFLAGS = -no-undefined -avoid-version -module
