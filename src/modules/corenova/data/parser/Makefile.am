INCLUDES = -I$(top_builddir)/src/include

# all modules built here should have this 
AM_LDFLAGS = -avoid-version -module -no-undefined

pkglibdir = $(libdir)/corenova/data/parser
pkglib_LTLIBRARIES =

#if BUILD_MYSQL

pkglib_LTLIBRARIES += gumboparser.la
mysql_la_SOURCES    = gumboparser.c
mysql_la_CFLAGS     = -DMODULE=corenova_data_parser_gumboparser @GUMBO_CFLAGS@
mysql_la_LIBADD     = @LIBGUMBO@

#endif


