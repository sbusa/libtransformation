INCLUDES = -I$(top_builddir)/src/include

# all modules built here should have this 
AM_LDFLAGS = -avoid-version -module -no-undefined

pkglibdir = $(libdir)/corenova/data/parser
pkglib_LTLIBRARIES =


pkglib_LTLIBRARIES += gumbo.la
gumbo_la_SOURCES    = gumbo.c
gumbo_la_CFLAGS     = -DMODULE=corenova_data_parser_gumbo 
gumbo_la_LIBADD     = @LIBGUMBO@

pkglib_LTLIBRARIES += jsonc.la
jsonc_la_SOURCES  = jsonc.c
jsonc_la_CFLAGS   = -DMODULE=corenova_data_parser_jsonc
