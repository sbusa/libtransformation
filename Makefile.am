###### Makefile.am - root Makefile
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src

EXTRA_DIST =									\
	corenova.pc.in								\
	src/programs/valgrind.sh.in

CLEANFILES =									\
	corenova.pc									\
	src/programs/valgrind.sh



pkgconfdir = $(libdir)/pkgconfig
pkgconf_DATA = corenova.pc

