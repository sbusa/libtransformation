INCLUDES = -I$(top_builddir)/src/include

# all modules built here should have this
AM_LDFLAGS = -avoid-version -module -no-undefined

pkglibdir = $(libdir)/corenova/sys
pkglib_LTLIBRARIES =

pkglib_LTLIBRARIES += debug.la
debug_la_SOURCES    = debug.c
debug_la_CFLAGS     = -DMODULE=corenova_sys_debug

pkglib_LTLIBRARIES += getopts.la
getopts_la_SOURCES  = getopts.c
getopts_la_CFLAGS   = -DMODULE=corenova_sys_getopts

pkglib_LTLIBRARIES += loader.la
loader_la_SOURCES   = loader.c
loader_la_CFLAGS    = -DMODULE=corenova_sys_loader
loader_la_LIBADD    = @LIBLTDL@

pkglib_LTLIBRARIES += quark.la
quark_la_SOURCES    = quark.c

quark_la_CFLAGS     = -DMODULE=corenova_sys_quark @PTHREAD_CFLAGS@
quark_la_LIBADD     = @LIBPTHREAD@

pkglib_LTLIBRARIES += signals.la
signals_la_SOURCES  = signals.c
signals_la_CFLAGS   = -DMODULE=corenova_sys_signals

pkglib_LTLIBRARIES  += transform.la
transform_la_SOURCES = transform.c
transform_la_CFLAGS  = -DMODULE=corenova_sys_transform

if BUILD_NFQUEUE

pkglib_LTLIBRARIES += nfqueue.la
nfqueue_la_SOURCES  = nfqueue.c
nfqueue_la_CFLAGS   = -DMODULE=corenova_sys_nfqueue
nfqueue_la_LIBADD   = @LIBNFQUEUE@ @LIBNETFILTER@

endif

if BUILD_WATCHDOG

pkglib_LTLIBRARIES += watchdog.la
watchdog_la_SOURCES = watchdog.c
watchdog_la_CFLAGS  = -DMODULE=corenova_sys_watchdog

endif

if BUILD_TCC

pkglib_LTLIBRARIES += compiler.la
compiler_la_SOURCES = compiler.c
compiler_la_CFLAGS  = -DMODULE=corenova_sys_compiler
compiler_la_LIBADD  = @LIBTCC@ 

endif
