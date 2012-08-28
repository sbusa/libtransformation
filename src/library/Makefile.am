INCLUDES = -I$(top_builddir)/src/include

lib_LTLIBRARIES    = libNovaLoader.la
libNovaLoader_la_SOURCES = NovaLoader.c
libNovaLoader_la_LIBADD  = @LIBLTDL@ @LIBPTHREAD@ $(top_builddir)/src/contrib/libubiqx.la
libNovaLoader_la_CFLAGS = -DDEFAULT_NOVAMODULE_PATH="\"$(libdir)\"" -I$(top_builddir)/src/contrib/

# do not make this a shared object?
#libNovaLoader_la_LDFLAGS = -no-undefined -module -avoid-version
