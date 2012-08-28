INCLUDES = -I$(top_builddir)/src/include

# all modules built here should have this 
AM_LDFLAGS = -avoid-version -module -no-undefined

pkglibdir = $(libdir)/corenova/data/db
pkglib_LTLIBRARIES =

if BUILD_MYSQL

pkglib_LTLIBRARIES += mysql.la
mysql_la_SOURCES    = mysql.c
mysql_la_CFLAGS     = -DMODULE=corenova_data_db_mysql @MYSQL_CFLAGS@
mysql_la_LIBADD     = @LIBMYSQL@

endif

if BUILD_PGSQL

pkglib_LTLIBRARIES += pgsql.la
pgsql_la_SOURCES    = pgsql.c
pgsql_la_CFLAGS     = -DMODULE=corenova_data_db_pgsql @PGSQL_CFLAGS@
pgsql_la_LIBADD     = @LIBPGSQL@

endif

