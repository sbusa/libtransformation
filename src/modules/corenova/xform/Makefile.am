INCLUDES = -I$(top_builddir)/src/include

# all modules built here should have this 
AM_LDFLAGS = -avoid-version -module -no-undefined

pkglibdir = $(libdir)/corenova/xform
pkglib_LTLIBRARIES =

pkglib_LTLIBRARIES += helloworld.la
helloworld_la_SOURCES    = helloworld.c
helloworld_la_CFLAGS     = -DMODULE=corenova_xform_helloworld

