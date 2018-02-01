plugindir = @NGFD_PLUGIN_DIR@
plugin_LTLIBRARIES = libngfd_null.la
libngfd_null_la_SOURCES = plugin.c
libngfd_null_la_LIBADD = @NGFD_PLUGIN_LIBS@
libngfd_null_la_LDFLAGS = -module -avoid-version
libngfd_null_la_CFLAGS = @NGFD_PLUGIN_CFLAGS@ -I$(top_srcdir)/src/include
