plugindir = @NGFD_PLUGIN_DIR@
plugin_LTLIBRARIES = libngfd_streamrestore.la
libngfd_streamrestore_la_SOURCES = plugin.c volume-controller.c
libngfd_streamrestore_la_LIBADD = @NGFD_PLUGIN_LIBS@ @DBUS_LIBS@
libngfd_streamrestore_la_LDFLAGS = -module -avoid-version
libngfd_streamrestore_la_CFLAGS = @NGFD_PLUGIN_CFLAGS@ @DBUS_CFLAGS@ @ROUTE_CFLAGS@ -I$(top_srcdir)/src/include
