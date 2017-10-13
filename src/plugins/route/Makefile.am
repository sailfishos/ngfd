plugindir = @NGFD_PLUGIN_DIR@
plugin_LTLIBRARIES = libngfd_route.la
libngfd_route_la_SOURCES = plugin.c
libngfd_route_la_LIBADD = @NGFD_PLUGIN_LIBS@ @DBUS_LIBS@
libngfd_route_la_LDFLAGS = -module -avoid-version
libngfd_route_la_CFLAGS = @NGFD_PLUGIN_CFLAGS@ @DBUS_CFLAGS@ @ROUTE_CFLAGS@ -I$(top_srcdir)/src/include
