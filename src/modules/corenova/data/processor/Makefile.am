INCLUDES = -I$(top_builddir)/src/include

# all modules built here should have this 
AM_LDFLAGS = -avoid-version -module -no-undefined

pkglibdir = $(libdir)/corenova/data/processor
pkglib_LTLIBRARIES =

pkglib_LTLIBRARIES += generic.la
generic_la_SOURCES  = generic.c
generic_la_CFLAGS   = -DMODULE=corenova_data_processor_generic

pkglib_LTLIBRARIES       += transformation.la
transformation_la_SOURCES = transformation.c
transformation_la_CFLAGS  = -DMODULE=corenova_data_processor_transformation
