INCLUDES = -I$(top_builddir)/src/include

# all modules built here should have this 
AM_LDFLAGS = -avoid-version -module -no-undefined

pkglibdir = $(libdir)/corenova/net/proto
pkglib_LTLIBRARIES =

##
# Protocol Modules

pkglib_LTLIBRARIES  += XFORMXFER.la
XFORMXFER_la_SOURCES = XFORMXFER.c
XFORMXFER_la_CFLAGS  = -DMODULE=corenova_net_proto_XFORMXFER


pkglib_LTLIBRARIES  += XFORMXFER_PLAIN.la
XFORMXFER_PLAIN_la_SOURCES = XFORMXFER_PLAIN.c
XFORMXFER_PLAIN_la_CFLAGS  = -DMODULE=corenova_net_proto_XFORMXFER_PLAIN

pkglib_LTLIBRARIES  += XFORMXFER2_PLAIN.la
XFORMXFER2_PLAIN_la_SOURCES = XFORMXFER2_PLAIN.c
XFORMXFER2_PLAIN_la_CFLAGS  = -DMODULE=corenova_net_proto_XFORMXFER2_PLAIN

pkglib_LTLIBRARIES  += XFORMXFER3.la
XFORMXFER3_la_SOURCES = XFORMXFER3.c
XFORMXFER3_la_CFLAGS  = -DMODULE=corenova_net_proto_XFORMXFER3

