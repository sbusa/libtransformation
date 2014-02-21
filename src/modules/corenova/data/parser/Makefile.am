INCLUDES = -I$(top_builddir)/src/include

# all modules built here should have this 
AM_LDFLAGS = -avoid-version -module -no-undefined

pkglibdir = $(libdir)/corenova/data/parser
pkglib_LTLIBRARIES =


pkglib_LTLIBRARIES += gumboparser.la
gumboparser_la_SOURCES    = gumboparser.c
gumboparser_la_CFLAGS     = -DMODULE=corenova_data_parser_gumboparser 


#pkglib_LTLIBRARIES += json-c.la
#jsonc_la_SOURCES  = json-c.c
#jsonc_la_CFLAGS   = -DMODULE=corenova_data_parser_json-c


