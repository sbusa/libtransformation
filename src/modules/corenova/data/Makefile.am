SUBDIRS = db processor parser

INCLUDES = -I$(top_builddir)/src/include

# all modules built here should have this 
AM_LDFLAGS = -avoid-version -module -no-undefined

pkglibdir = $(libdir)/corenova/data
pkglib_LTLIBRARIES =

##
# Data storage utilities

pkglib_LTLIBRARIES += array.la
array_la_SOURCES    = array.c
array_la_CFLAGS     = -DMODULE=corenova_data_array

## deprecated...
# pkglib_LTLIBRARIES += data.la
# data_la_SOURCES     = data.c
# data_la_CFLAGS      = -DMODULE=data

pkglib_LTLIBRARIES += database.la
database_la_SOURCES = database.c
database_la_CFLAGS  = -DMODULE=corenova_data_database

pkglib_LTLIBRARIES += list.la
list_la_SOURCES     = list.c
list_la_CFLAGS      = -DMODULE=corenova_data_list

pkglib_LTLIBRARIES += message.la
message_la_SOURCES  = message.c
message_la_CFLAGS   = -DMODULE=corenova_data_message

pkglib_LTLIBRARIES += object.la
object_la_SOURCES   = object.c
object_la_CFLAGS    = -DMODULE=corenova_data_object

pkglib_LTLIBRARIES += queue.la
queue_la_SOURCES    = queue.c
queue_la_CFLAGS     = -DMODULE=corenova_data_queue

##
# Data manipulation utilities

pkglib_LTLIBRARIES  += processor.la
processor_la_SOURCES = processor.c
processor_la_CFLAGS  = -DMODULE=corenova_data_processor

pkglib_LTLIBRARIES += string.la
string_la_SOURCES   = string.c
string_la_CFLAGS    = -DMODULE=corenova_data_string

pkglib_LTLIBRARIES += md5.la
md5_la_SOURCES      = md5.c
md5_la_CFLAGS       = -DMODULE=corenova_data_md5

pkglib_LTLIBRARIES += pipe.la
pipe_la_SOURCES     = pipe.c
pipe_la_CFLAGS      = -DMODULE=corenova_data_pipe

##
# Data searching libraries

pkglib_LTLIBRARIES += cache.la
cache_la_SOURCES  = cache.c
cache_la_CFLAGS   = -DMODULE=corenova_data_cache -I$(top_builddir)/src/contrib/
cache_la_LIBADD   = $(top_builddir)/src/contrib/libubiqx.la

pkglib_LTLIBRARIES += stree.la
stree_la_SOURCES = stree.c
stree_la_CFLAGS  = -DMODULE=corenova_data_stree

pkglib_LTLIBRARIES += hmap.la
hmap_la_SOURCES = hmap.c
hmap_la_CFLAGS  = -DMODULE=corenova_data_hmap

##
# File related Modules

pkglib_LTLIBRARIES += file.la
file_la_SOURCES     = file.c
file_la_CFLAGS      = -DMODULE=corenova_data_file

pkglib_LTLIBRARIES += glob.la
glob_la_SOURCES     = glob.c
glob_la_CFLAGS      = -DMODULE=corenova_data_glob

pkglib_LTLIBRARIES += inifile.la
inifile_la_SOURCES  = inifile.c
inifile_la_CFLAGS   = -DMODULE=corenova_data_inifile

pkglib_LTLIBRARIES += spool.la
spool_la_SOURCES    = spool.c
spool_la_CFLAGS   = -DMODULE=corenova_data_spool

pkglib_LTLIBRARIES += streams.la
streams_la_SOURCES    = streams.c
streams_la_CFLAGS   = -DMODULE=corenova_data_streams

##
# Configuration state Data Holders

pkglib_LTLIBRARIES += category.la
category_la_SOURCES = category.c
category_la_CFLAGS  = -DMODULE=corenova_data_category

pkglib_LTLIBRARIES      += configuration.la
configuration_la_SOURCES = configuration.c
configuration_la_CFLAGS  = -DMODULE=corenova_data_configuration

pkglib_LTLIBRARIES    += parameters.la
parameters_la_SOURCES  = parameters.c
parameters_la_CFLAGS   = -DMODULE=corenova_data_parameters

