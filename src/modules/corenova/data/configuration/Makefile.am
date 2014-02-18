INCLUDES = -I$(top_builddir)/src/include

# all modules built here should have this 
AM_LDFLAGS = -avoid-version -module -no-undefined

pkglibdir = $(libdir)/corenova/data/configuration
pkglib_LTLIBRARIES =

pkglib_LTLIBRARIES += ini.la
ini_la_SOURCES  = ini.c
ini_la_CFLAGS   = -DMODULE=corenova_data_configuration_ini

pkglib_LTLIBRARIES += xform.la
xform_la_SOURCES    = xform.c
xform_la_CFLAGS  = -DMODULE=corenova_data_configuration_xform
